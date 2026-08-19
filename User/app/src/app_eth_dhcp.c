/**
 * @file    app_eth_dhcp.c
 * @brief   DHCP 客户端控制线程
 *
 * @note    等 link UP 后做首次 apply_config；主循环监听 cfg 变更和 link UP 跳变，
 *          DHCP 失败回退静态。fallback / 静态 模式下 link UP 跳变不自动重试
 */
#include "app_eth_dhcp.h"
#include "app_eth_config.h"
#include "app_eth_link_check.h"
#include "tx_api.h"
#include "nx_api.h"
#include "nxd_dhcp_client.h"
#include "threadx_printf.h"

#define THREAD_ETH_DHCP_PRIO       10u
#define THREAD_ETH_DHCP_STACK_SIZE 2048u

#define POLL_INTERVAL              (NX_IP_PERIODIC_RATE / 2)

static TX_THREAD thread_eth_dhcp_tcb;
static uint64_t thread_eth_dhcp_stack[THREAD_ETH_DHCP_STACK_SIZE / 8];

static NX_DHCP s_dhcp;
static UCHAR s_dhcp_created = NX_FALSE;

extern NX_IP ip;
extern NX_PACKET_POOL eth_pool;

static void thread_eth_dhcp_entry(ULONG thread_input);

/**
 * @brief  创建 DHCP 客户端控制线程
 * @note   thread_eth_entry 在 IP/ARP/TCP/UDP 启用后调用一次。
 *         调用前必须已 app_eth_config_init（由 thread_eth_entry 保证）
 */
void app_eth_dhcp_create(void)
{
    tx_thread_create(&thread_eth_dhcp_tcb, /* 任务控制块地址 */
                     "App ETH DHCP", /* 任务名 */
                     thread_eth_dhcp_entry, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &thread_eth_dhcp_stack[0], /* 堆栈基地址 */
                     THREAD_ETH_DHCP_STACK_SIZE, /* 堆栈空间大小 */
                     THREAD_ETH_DHCP_PRIO, /* 任务优先级 */
                     THREAD_ETH_DHCP_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */
}

/* 把主机序 IP 拆成 4 字节打印 */
#define IP_QUAD(addr) \
	((addr) >> 24) & 0xFF, ((addr) >> 16) & 0xFF, \
	((addr) >>  8) & 0xFF,  (addr)        & 0xFF

/**
 * @brief  打印当前 lease 到 Netx_Terminal
 * @param  tag     前缀标签（dhcp bound / static / dhcp fallback static 等）
 * @param  ip_addr 主机序 IP
 * @param  mask    子网掩码
 * @param  gw      网关
 * @param  dns1    主 DNS
 * @param  dns2    备 DNS
 */
static void print_lease(const char *tag, uint32_t ip_addr, uint32_t mask,
                        uint32_t gw, uint32_t dns1, uint32_t dns2)
{
    threadx_printf(Netx_Terminal,
                   "[dhcp] %s: ip=%lu.%lu.%lu.%lu mask=%lu.%lu.%lu.%lu "
                   "gw=%lu.%lu.%lu.%lu dns1=%lu.%lu.%lu.%lu "
                   "dns2=%lu.%lu.%lu.%lu\r\n",
                   tag,
                   IP_QUAD(ip_addr), IP_QUAD(mask),
                   IP_QUAD(gw), IP_QUAD(dns1),
                   IP_QUAD(dns2));
}

/**
 * @brief  阻塞等 DHCP 状态机进入 BOUND
 * @param  timeout_ticks 等待 tick 数
 * @return NX_SUCCESS（已 BOUND）/ NX_NOT_BOUND（超时）
 * @note   NetXDuo 的 DHCP 工作线程在后台持续发包，本函数只是旁观状态字段；
 *         即便超时返回，DHCP 线程仍在跑
 */
static UINT dhcp_wait_bound(ULONG timeout_ticks)
{
    ULONG elapsed = 0;

    while (elapsed < timeout_ticks) {
        if (s_dhcp.nx_dhcp_interface_record[0].nx_dhcp_state
            == NX_DHCP_STATE_BOUND)
            return NX_SUCCESS;

        tx_thread_sleep(POLL_INTERVAL);
        elapsed += POLL_INTERVAL;
    }

    return NX_NOT_BOUND;
}

/**
 * @brief  应用静态 IP 配置
 * @param  cfg 配置源（用其中的 ip/mask/gateway/dns）
 * @param  src 上报 status 用的 IP 来源（STATIC 或 DHCP_FALLBACK）
 * @param  tag 打印 lease 时的前缀标签
 * @note   如果当前有运行中的 DHCP 客户端会先停掉
 */
static void apply_static(const struct app_eth_config *cfg,
                         enum app_eth_ip_source src, const char *tag)
{
    UINT status;

    /* 静态 IP 不需要 DHCP 客户端，确保停掉 */
    if (s_dhcp_created == NX_TRUE) {
        nx_dhcp_stop(&s_dhcp);
        nx_dhcp_delete(&s_dhcp);
        s_dhcp_created = NX_FALSE;
    }

    status = nx_ip_interface_address_set(&ip, 0, cfg->ip, cfg->mask);
    if (status != NX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[dhcp] static ip set failed: 0x%X\r\n",
                       status);
        return;
    }

    nx_ip_gateway_address_set(&ip, cfg->gateway);

    _app_eth_status_set(src, cfg->ip, cfg->mask, cfg->gateway,
                        cfg->dns1, cfg->dns2);

    print_lease(tag, cfg->ip, cfg->mask, cfg->gateway,
                cfg->dns1, cfg->dns2);
}

/**
 * @brief  尝试通过 DHCP 取地址，失败回退静态
 * @param  cfg 配置源（DHCP 超时由 cfg->dhcp_timeout_ms 决定）
 * @note   timeout 内成功 -> BOUND；失败 -> 走 apply_static 上报 DHCP_FALLBACK。
 *         fallback 状态保留到下次 apply 或复位（link UP 跳变不自动重试）
 */
static void try_dhcp(const struct app_eth_config *cfg)
{
    ULONG ip_addr = 0, mask = 0, gateway = 0;
    ULONG dns_addr = 0;
    UINT dns_size = sizeof(dns_addr);
    ULONG timeout_ticks;
    UINT status;
    uint32_t dns1 = 0;

    /* 先确保旧实例清掉，apply 重入时也要重新走 DISCOVER */
    if (s_dhcp_created == NX_TRUE) {
        nx_dhcp_stop(&s_dhcp);
        nx_dhcp_delete(&s_dhcp);
        s_dhcp_created = NX_FALSE;
    }

    status = nx_dhcp_create(&s_dhcp, &ip, "stm32h7-dhcp-client");
    if (status != NX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[dhcp] create failed: 0x%X\r\n", status);
        goto fallback;
    }
    s_dhcp_created = NX_TRUE;

    status = nx_dhcp_packet_pool_set(&s_dhcp, &eth_pool);
    if (status != NX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[dhcp] packet pool set failed: 0x%X\r\n", status);
        nx_dhcp_delete(&s_dhcp);
        s_dhcp_created = NX_FALSE;
        goto fallback;
    }

    status = nx_dhcp_start(&s_dhcp);
    if (status != NX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[dhcp] start failed: 0x%X\r\n", status);
        goto fallback;
    }

    timeout_ticks = (cfg->dhcp_timeout_ms != 0 ?
                         cfg->dhcp_timeout_ms :
                         APP_ETH_DHCP_TIMEOUT_DEFAULT_MS)
                    * NX_IP_PERIODIC_RATE / 1000;

    if (dhcp_wait_bound(timeout_ticks) != NX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[dhcp] bound timeout, fallback to static\r\n");
        goto fallback;
    }

    nx_ip_address_get(&ip, &ip_addr, &mask);
    nx_ip_gateway_address_get(&ip, &gateway);

    if (nx_dhcp_user_option_retrieve(&s_dhcp, NX_DHCP_OPTION_DNS_SVR,
                                     (UCHAR *)&dns_addr, &dns_size)
        == NX_SUCCESS)
        dns1 = (uint32_t)dns_addr;

    _app_eth_status_set(APP_ETH_IP_DHCP,
                        (uint32_t)ip_addr, (uint32_t)mask,
                        (uint32_t)gateway, dns1, 0);

    print_lease("dhcp bound", (uint32_t)ip_addr, (uint32_t)mask,
                (uint32_t)gateway, dns1, 0);
    return;

fallback:
    apply_static(cfg, APP_ETH_IP_DHCP_FALLBACK, "dhcp fallback static");
}

/**
 * @brief  按 cfg.dhcp_enable 分派到 DHCP 或静态路径
 * @param  cfg 配置源
 */
static void apply_config(const struct app_eth_config *cfg)
{
    if (cfg->dhcp_enable)
        try_dhcp(cfg);
    else
        apply_static(cfg, APP_ETH_IP_STATIC, "static");
}

/**
 * @brief  DHCP 控制线程入口
 * @param  thread_input 未使用
 * @note   等 link UP 后做首次 apply_config，之后主循环监听两类事件：
 *         cfg 变更（来自 app_eth_config_apply）→ 重新 apply；
 *         link UP 跳变 → DHCP 模式下重启 DISCOVER（静态/fallback 不重试）
 */
static void thread_eth_dhcp_entry(ULONG thread_input)
{
    struct app_eth_config cfg;
    struct app_eth_config new_cfg;
    struct link_sub link_sub;
    enum app_eth_link_state link_state;
    UINT status;

    (void)thread_input;

    /* 拿当前 cfg 做首次应用 */
    _app_eth_config_snapshot(&cfg);

    /* 等 link up 再开搞，避免在 down 上浪费 DHCP DISCOVER 重传 */
    app_eth_link_wait_up(TX_WAIT_FOREVER);

    apply_config(&cfg);

    /*
     * 主循环：同时关注 cfg 变化 和 link UP 跳变。
     * 没有统一的事件组，用"等 cfg + 短超时后查 link sub"。
     * 这条路径不是热路径，500ms 延迟可接受。
     */
    app_eth_link_sub_init(&link_sub);

    while (1) {
        status = _app_eth_config_wait_pending(&new_cfg, POLL_INTERVAL);

        if (status == NX_SUCCESS) {
            threadx_printf(Netx_Terminal,
                           "[dhcp] config changed, re-apply\r\n");
            cfg = new_cfg;
            apply_config(&cfg);
            continue;
        }

        /* 没有 cfg 变化，看看 link 有没有跳变 */
        status = app_eth_link_wait_change(&link_sub, &link_state,
                                          TX_NO_WAIT);
        if (status != NX_SUCCESS)
            continue;

        if (link_state != APP_ETH_LINK_UP)
            continue;

        /*
         * link UP 跳变（拔插或换网）。仅 DHCP 模式下重启 DISCOVER；
         * fallback / 静态 不自动重试，按设计约定。
         */
        if (cfg.dhcp_enable) {
            threadx_printf(Netx_Terminal,
                           "[dhcp] link up, restart DISCOVER\r\n");
            try_dhcp(&cfg);
        }
    }
}

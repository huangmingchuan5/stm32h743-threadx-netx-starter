/**
 * @file    app_eth_config.c
 * @brief   app_eth_config.h 公开/内部 API 的实现
 *
 * @note    三份数据：current（DHCP 线程正在用的）、pending（apply 写入待消费）、
 *          status（DHCP 线程汇报的运行时状态）。统一 mutex 串行化。
 *          apply 仅写 pending + 发事件立即返回，真正切换在 DHCP 线程上下文做
 */
#include "app_eth_config.h"
#include "stm32h7xx_hal.h"
#include "nx_api.h"
#include <string.h>

/*
 * 网络配置 / 状态的中心持有者。
 *
 * 设计：
 *   - 三份数据：current（DHCP 线程正在用的）、pending（apply 写入、待消费）、
 *     status（DHCP 线程汇报的运行时状态）
 *   - 写者多读者少，统一用 mutex 串行化
 *   - apply 仅写 pending + 发事件，立即返回 —— 真正的 IP 切换/DHCP 重启
 *     在 DHCP 线程上下文里做，避免 apply 调用者被阻塞
 */

#define EVT_CFG_PENDING (1UL << 0)

static TX_MUTEX s_lock;
static TX_EVENT_FLAGS_GROUP s_events;

static struct app_eth_config s_current;
static struct app_eth_config s_pending;
static UCHAR s_has_pending = NX_FALSE;

static struct app_eth_status s_status;

static UCHAR s_inited = NX_FALSE;

/**
 * @brief  把内置默认 cfg 填到 *cfg
 * @param  cfg 待填充的 cfg 结构体；NX_NULL 时直接返回
 * @note   默认 DHCP 启用，静态回退 192.168.2.233/24 gw=.1，DNS 8.8.8.8/8.8.4.4，
 *         MAC OUI 00:80:E1（ST 分配）
 */
void app_eth_config_get_default(struct app_eth_config *cfg)
{
    if (cfg == NX_NULL)
        return;

    memset(cfg, 0, sizeof(*cfg));

    cfg->magic = APP_ETH_CONFIG_MAGIC;
    cfg->version = APP_ETH_CONFIG_VERSION;

    cfg->dhcp_enable = 1;
    cfg->dhcp_timeout_ms = APP_ETH_DHCP_TIMEOUT_DEFAULT_MS;

    cfg->ip = IP_ADDRESS(192, 168, 2, 233);
    cfg->mask = 0xFFFFFF00UL;
    cfg->gateway = IP_ADDRESS(192, 168, 2, 1);
    cfg->dns1 = IP_ADDRESS(8, 8, 8, 8);
    cfg->dns2 = IP_ADDRESS(8, 8, 4, 4);

    /* 默认 MAC（OUI 00:80:E1 是 STMicroelectronics 的） */
    cfg->mac[0] = 0x00;
    cfg->mac[1] = 0x80;
    cfg->mac[2] = 0xE1;
    cfg->mac[3] = 0x00;
    cfg->mac[4] = 0x00;
    cfg->mac[5] = 0x00;

    /* crc32 字段暂不计算，等加 flash 持久化时再补 */
}

/**
 * @brief  初始化 cfg/status/同步原语，并把 cfg 填成默认值
 * @note   thread_eth_entry 在 nx_ip_create 之前调用一次。重复调用是 no-op
 */
void app_eth_config_init(void)
{
    if (s_inited == NX_TRUE)
        return;

    tx_mutex_create(&s_lock, "eth cfg lock", TX_NO_INHERIT);
    tx_event_flags_create(&s_events, "eth cfg events");

    app_eth_config_get_default(&s_current);
    memset(&s_pending, 0, sizeof(s_pending));
    memset(&s_status, 0, sizeof(s_status));

    s_has_pending = NX_FALSE;
    s_inited = NX_TRUE;
}

/**
 * @brief  取当前正在用的 cfg 快照
 * @param  cfg 输出，NX_NULL 或未初始化时直接返回
 * @note   读时持锁，调用方可在任意线程上下文使用
 */
void app_eth_config_get_current(struct app_eth_config *cfg)
{
    if (cfg == NX_NULL || s_inited == NX_FALSE)
        return;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    *cfg = s_current;
    tx_mutex_put(&s_lock);
}

/**
 * @brief  取当前运行时状态快照（IP 来源、ip/mask/gw/dns、link）
 * @param  out 输出，NX_NULL 或未初始化时直接返回
 * @note   link 字段实时从 link_check 模块取，不受本模块的锁保护
 */
void app_eth_status_get(struct app_eth_status *out)
{
    if (out == NX_NULL || s_inited == NX_FALSE)
        return;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    *out = s_status;
    tx_mutex_put(&s_lock);

    /* link 字段不放在锁里维护，直接现读 link_check 模块 */
    out->link = app_eth_link_get_state();
}

/**
 * @brief  cfg 参数合法性校验
 * @param  cfg 待校验
 * @return NX_SUCCESS / NX_NOT_SUCCESSFUL
 * @note   规则：magic+version 匹配，ip/mask 非 0，mask 是连续 1+连续 0，
 *         ip 与 gateway 同子网（gateway 为 0 则跳过该项）。
 *         DHCP 模式下静态字段仍按回退用途校验
 */
static UINT cfg_validate(const struct app_eth_config *cfg)
{
    uint32_t mask = cfg->mask;
    uint32_t inv = ~mask + 1;

    if (cfg->magic != APP_ETH_CONFIG_MAGIC)
        return NX_NOT_SUCCESSFUL;
    if (cfg->version != APP_ETH_CONFIG_VERSION)
        return NX_NOT_SUCCESSFUL;
    if (cfg->ip == 0 || mask == 0)
        return NX_NOT_SUCCESSFUL;
    if ((mask & (inv - 1)) != 0) /* 掩码非连续 */
        return NX_NOT_SUCCESSFUL;
    if ((cfg->ip & mask) != (cfg->gateway & mask) && cfg->gateway != 0)
        return NX_NOT_SUCCESSFUL;

    return NX_SUCCESS;
}

/**
 * @brief  应用新配置（异步）
 * @param  cfg 新配置
 * @return NX_SUCCESS / NX_NOT_SUCCESSFUL（cfg 非法或未初始化）
 * @note   仅做参数校验并把 cfg 提交到 pending，立即返回。DHCP 线程在下一次
 *         循环中取走并真正应用。
 *         若新 cfg 的 MAC 与当前不同，本函数会调 NVIC_SystemReset 不返回
 */
UINT app_eth_config_apply(const struct app_eth_config *cfg)
{
    UCHAR mac_changed;

    if (cfg == NX_NULL || s_inited == NX_FALSE)
        return NX_NOT_SUCCESSFUL;

    if (cfg_validate(cfg) != NX_SUCCESS)
        return NX_NOT_SUCCESSFUL;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    mac_changed = (memcmp(cfg->mac, s_current.mac, sizeof(cfg->mac)) != 0) ?
                      NX_TRUE :
                      NX_FALSE;
    tx_mutex_put(&s_lock);

    if (mac_changed == NX_TRUE) {
        /*
         * MAC 无法热切换（ETH 外设初始化时已写入），软复位让 bsp_eth
         * 重新读 cfg。持久化未落地前复位后会回到默认 MAC。
         */
        NVIC_SystemReset();
        /* not reached */
    }

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    s_pending = *cfg;
    s_has_pending = NX_TRUE;
    tx_mutex_put(&s_lock);

    tx_event_flags_set(&s_events, EVT_CFG_PENDING, TX_OR);

    return NX_SUCCESS;
}

/**
 * @brief  DHCP 线程内部用：取当前 cfg 副本
 * @param  out 输出，NX_NULL 直接返回
 * @note   仅供 app_eth_dhcp 模块调用，应用层请用 app_eth_config_get_current
 */
void _app_eth_config_snapshot(struct app_eth_config *out)
{
    if (out == NX_NULL)
        return;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    *out = s_current;
    tx_mutex_put(&s_lock);
}

/**
 * @brief  DHCP 线程内部用：阻塞等待 pending cfg
 * @param  out        新 cfg 输出
 * @param  wait_ticks 等待 tick 数
 * @return NX_SUCCESS（拿到新 cfg）/ TX_NO_EVENTS（超时或被别人吃掉）/
 *         NX_NOT_SUCCESSFUL（out 为 NX_NULL）
 * @note   返回 NX_SUCCESS 时 s_current 已同步成 s_pending
 */
UINT _app_eth_config_wait_pending(struct app_eth_config *out, ULONG wait_ticks)
{
    ULONG actual;
    UINT status;

    if (out == NX_NULL)
        return NX_NOT_SUCCESSFUL;

    status = tx_event_flags_get(&s_events, EVT_CFG_PENDING, TX_OR_CLEAR,
                                &actual, wait_ticks);
    if (status != TX_SUCCESS)
        return status;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    if (s_has_pending == NX_TRUE) {
        *out = s_pending;
        s_current = s_pending;
        s_has_pending = NX_FALSE;
        status = NX_SUCCESS;
    } else {
        /* 事件来了但 pending 被别人吃掉了 —— 当作 spurious wake */
        status = TX_NO_EVENTS;
    }
    tx_mutex_put(&s_lock);

    return status;
}

/**
 * @brief  DHCP 线程内部用：汇报当前生效的 IP 来源与地址
 * @param  src     IP 来源（STATIC / DHCP / DHCP_FALLBACK）
 * @param  ip      主机序 IP
 * @param  mask    子网掩码
 * @param  gateway 默认网关
 * @param  dns1    主 DNS
 * @param  dns2    备 DNS
 */
void _app_eth_status_set(enum app_eth_ip_source src,
                         uint32_t ip, uint32_t mask, uint32_t gateway,
                         uint32_t dns1, uint32_t dns2)
{
    if (s_inited == NX_FALSE)
        return;

    tx_mutex_get(&s_lock, TX_WAIT_FOREVER);
    s_status.ip_source = src;
    s_status.ip = ip;
    s_status.mask = mask;
    s_status.gateway = gateway;
    s_status.dns1 = dns1;
    s_status.dns2 = dns2;
    tx_mutex_put(&s_lock);
}
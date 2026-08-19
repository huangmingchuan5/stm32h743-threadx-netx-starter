/**
 * @file    app_eth.c
 * @brief   ETH 主线程 thread_eth_entry：拉起 NetX IP 实例并孵化
 *          link_check / dhcp / tcp_echo 三条业务线程；附带状态/统计信息表
 *
 * @note    对外接口契约见 app_eth.h。本文件还托管 NetX IP 线程栈和包池。
 */
#include "app_eth.h"
#include <stdio.h>
#include "tx_api.h"
#include "nx_api.h"
#include "nx_stm32_eth_driver.h"
#include "threadx_printf.h"
#include "app_eth_link_check.h"
#include "app_eth_dhcp.h"
#include "app_eth_config.h"
#include "app_tcp_echo.h"

#define THREAD_ETH_PRIO 10u
#define THREAD_ETH_STACK_SIZE 4096u
static TX_THREAD thread_eth_tcb;
static uint64_t thread_eth_stack[THREAD_ETH_STACK_SIZE / 8];

void thread_eth_entry(ULONG thread_input);

void app_eth_info_print(void);

/**
 * @brief  创建 ETH 主线程
 * @note   由 AppTaskCreate 在内核启动后调用一次
 */
void thread_eth_create(void)
{
    /**************创建ETH任务*********************/
    tx_thread_create(&thread_eth_tcb, /* 任务控制块地址 */
                     "App ETH", /* 任务名 */
                     thread_eth_entry, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &thread_eth_stack[0], /* 堆栈基地址 */
                     THREAD_ETH_STACK_SIZE, /* 堆栈空间大小 */
                     THREAD_ETH_PRIO, /* 任务优先级*/
                     THREAD_ETH_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */
}

NX_PACKET_POOL eth_pool;
NX_IP ip;

#define ETH_PACKET_SIZE 1536
#define NX_PACKET_POOL_SIZE ((ETH_PACKET_SIZE + sizeof(NX_PACKET)) * 30)

ULONG packet_pool_area[NX_PACKET_POOL_SIZE / 4 + 4]
    __attribute__((section(".NetXPoolSection"), aligned(32)));

#define THREAD_NETX_PRIO 2
#define THREAD_NETX_STACK_SIZE 4096u
static uint64_t thread_netx_stack[THREAD_NETX_STACK_SIZE / 8];

/* ARP缓存 */
ULONG arp_space_area[512 / sizeof(ULONG)];

/**
 * @brief  ETH 主线程入口
 * @param  thread_input 未使用
 * @note   按依赖顺序初始化 config / packet pool / IP / ARP / ICMP / UDP / TCP，
 *         再拉起 link_check、dhcp、tcp_echo 三条业务线程，最后进入信息表
 *         刷新循环（每 NX_IP_PERIODIC_RATE 一次）
 */
void thread_eth_entry(ULONG thread_input)
{
    (void)thread_input;

    /*
     * config 必须先初始化：nx_ip_create 内部会触发 bsp_eth 初始化 ETH 外设
     * 并写入 MAC，而 MAC 从 cfg 读，因此 cfg 必须在 nx_ip_create 之前就绪。
     */
    app_eth_config_init();

    nx_system_initialize();

    nx_packet_pool_create(&eth_pool, "NetX Main Packet Pool", 1536,
                          packet_pool_area,
                          sizeof(packet_pool_area));

    /*
     * IP/mask 用 0 创建，真正的地址由 app_eth_dhcp 线程根据 config
     * 决定（DHCP 拿到 或 写入静态）。
     */
    nx_ip_create(&ip,
                 "NetX IP Instance",
                 0, 0,
                 &eth_pool, nx_stm32_eth_driver,
                 (UCHAR*)thread_netx_stack,
                 sizeof(thread_netx_stack),
                 THREAD_NETX_PRIO);

    nx_arp_enable(&ip, (void *)arp_space_area, sizeof(arp_space_area));

    nx_ip_fragment_enable(&ip);

    nx_icmp_enable(&ip);

    nx_udp_enable(&ip);

    nx_tcp_enable(&ip);

    app_eth_link_check_create();

    app_eth_dhcp_create();

    app_tcp_echo_create();

    /* 主循环：每秒刷新一次以太网状态表到 Net_Info_Terminal */
    while (1) {
        app_eth_info_print();
        tx_thread_sleep(NX_IP_PERIODIC_RATE);
    }
}

/* ============================== 信息表 ============================== */

/**
 * @brief  IP 来源枚举转可打印字符串
 * @param  src IP 来源枚举值
 * @return 对应的字符串常量，未知值返回 "NONE"
 */
static const char *ip_source_str(enum app_eth_ip_source src)
{
    switch (src) {
    case APP_ETH_IP_STATIC:
        return "STATIC";
    case APP_ETH_IP_DHCP:
        return "DHCP";
    case APP_ETH_IP_DHCP_FALLBACK:
        return "DHCP-FALLBACK";
    case APP_ETH_IP_NONE:
    default:
        return "NONE";
    }
}

/**
 * @brief  把主机序 IP 格式化为 "xxx.xxx.xxx.xxx" 字符串
 * @param  addr  主机序 IP 地址
 * @param  buf   输出缓冲区
 * @param  bufsz buf 容量（至少 16 字节，含 '\0'）
 */
static void ip_to_str(uint32_t addr, char *buf, size_t bufsz)
{
    snprintf(buf, bufsz, "%u.%u.%u.%u",
             (unsigned)((addr >> 24) & 0xFF),
             (unsigned)((addr >> 16) & 0xFF),
             (unsigned)((addr >> 8) & 0xFF),
             (unsigned)(addr & 0xFF));
}

/**
 * @brief  整屏刷新 Net_Info_Terminal 上的以太网状态/统计表
 * @note   thread_eth_entry 每 NX_IP_PERIODIC_RATE 调一次
 */
void app_eth_info_print(void)
{
    struct app_eth_status st;
    struct app_eth_config cfg;
    char ip_s[16], mask_s[16], gw_s[16], dns1_s[16], dns2_s[16];

    /* 基本信息源 */
    app_eth_status_get(&st);
    app_eth_config_get_current(&cfg);

    /* IP 协议层统计 */
    ULONG ip_tx_pkt = 0, ip_tx_byte = 0, ip_rx_pkt = 0, ip_rx_byte = 0;
    ULONG ip_invalid = 0, ip_rx_drop = 0;
    nx_ip_info_get(&ip,
                   &ip_tx_pkt, &ip_tx_byte,
                   &ip_rx_pkt, &ip_rx_byte,
                   &ip_invalid, &ip_rx_drop,
                   NX_NULL, NX_NULL, NX_NULL, NX_NULL);

    /* TCP / UDP */
    ULONG tcp_tx_pkt = 0, tcp_tx_byte = 0, tcp_rx_pkt = 0, tcp_rx_byte = 0;
    nx_tcp_info_get(&ip,
                    &tcp_tx_pkt, &tcp_tx_byte,
                    &tcp_rx_pkt, &tcp_rx_byte,
                    NX_NULL, NX_NULL, NX_NULL, NX_NULL,
                    NX_NULL, NX_NULL, NX_NULL);

    ULONG udp_tx_pkt = 0, udp_tx_byte = 0, udp_rx_pkt = 0, udp_rx_byte = 0;
    nx_udp_info_get(&ip,
                    &udp_tx_pkt, &udp_tx_byte,
                    &udp_rx_pkt, &udp_rx_byte,
                    NX_NULL, NX_NULL, NX_NULL);

    /* ICMP / ARP
     * nx_icmp_info_get 只给"主动 ping 别人"的统计；
     * "被别人 ping"的次数得直接读 IP 实例字段。 */
    ULONG ping_sent = 0, ping_resp = 0;
    nx_icmp_info_get(&ip, &ping_sent, NX_NULL, NX_NULL,
                     &ping_resp, NX_NULL, NX_NULL);
    ULONG ping_recv = ip.nx_ip_pings_received;
    ULONG ping_responded = ip.nx_ip_pings_responded_to;

    ULONG arp_req_s = 0, arp_req_r = 0, arp_rsp_s = 0, arp_rsp_r = 0;
    nx_arp_info_get(&ip, &arp_req_s, &arp_req_r, &arp_rsp_s, &arp_rsp_r,
                    NX_NULL, NX_NULL, NX_NULL, NX_NULL);

    /* Packet pool */
    ULONG pool_total = 0, pool_free = 0, pool_empty_req = 0;
    ULONG pool_suspend = 0, pool_invalid = 0;
    nx_packet_pool_info_get(&eth_pool, &pool_total, &pool_free,
                            &pool_empty_req, &pool_suspend, &pool_invalid);

    ip_to_str(st.ip, ip_s, sizeof(ip_s));
    ip_to_str(st.mask, mask_s, sizeof(mask_s));
    ip_to_str(st.gateway, gw_s, sizeof(gw_s));
    ip_to_str(st.dns1, dns1_s, sizeof(dns1_s));
    ip_to_str(st.dns2, dns2_s, sizeof(dns2_s));

    /* 表格行模板："| " + 65 字符左对齐内容 + " |\r\n"。边框横线 67 个 "-"。 */
    char line[72];

    /* 整屏刷新 */
    threadx_printf_clear(Net_Info_Terminal);

    threadx_printf(Net_Info_Terminal,
                   "+--------------------------- ETH STATUS ----------------------------+\r\n");

    snprintf(line, sizeof(line),
             "Link    : %-15s    Source  : %s",
             (st.link == APP_ETH_LINK_UP) ? "UP" : "DOWN",
             ip_source_str(st.ip_source));
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line), "IP      : %-15s    Mask    : %s", ip_s,
             mask_s);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "Gateway : %-15s    MAC     : %02X:%02X:%02X:%02X:%02X:%02X",
             gw_s,
             cfg.mac[0], cfg.mac[1], cfg.mac[2],
             cfg.mac[3], cfg.mac[4], cfg.mac[5]);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line), "DNS1    : %-15s    DNS2    : %s", dns1_s,
             dns2_s);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    threadx_printf(Net_Info_Terminal,
                   "+--------------------- PACKETS (sent / recv) -----------------------+\r\n");

    snprintf(line, sizeof(line),
             "IP    pkt: %8lu /%8lu   bytes: %8lu /%8lu",
             ip_tx_pkt, ip_rx_pkt, ip_tx_byte, ip_rx_byte);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "IP    invalid: %8lu   rx_dropped: %8lu",
             ip_invalid, ip_rx_drop);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "TCP   pkt: %8lu /%8lu   bytes: %8lu /%8lu",
             tcp_tx_pkt, tcp_rx_pkt, tcp_tx_byte, tcp_rx_byte);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "UDP   pkt: %8lu /%8lu   bytes: %8lu /%8lu",
             udp_tx_pkt, udp_rx_pkt, udp_tx_byte, udp_rx_byte);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "ICMP  out sent=%-6lu resp=%-6lu  in recv=%-6lu resp=%lu",
             ping_sent, ping_resp, ping_recv, ping_responded);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    snprintf(line, sizeof(line),
             "ARP   req s/r=%lu/%lu   rsp s/r=%lu/%lu",
             arp_req_s, arp_req_r, arp_rsp_s, arp_rsp_r);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    threadx_printf(Net_Info_Terminal,
                   "+------------------------- PACKET POOL -----------------------------+\r\n");

    snprintf(line, sizeof(line),
             "total=%lu  free=%lu  empty_req=%lu  suspended=%lu  invalid=%lu",
             pool_total, pool_free, pool_empty_req, pool_suspend, pool_invalid);
    threadx_printf(Net_Info_Terminal, "| %-65s |\r\n", line);

    threadx_printf(Net_Info_Terminal,
                   "+-------------------------------------------------------------------+\r\n");
}

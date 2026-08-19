/**
 * @file    app_eth_dhcp.h
 * @brief   DHCP 客户端控制线程入口
 */
#ifndef STM32F4_TX_NETX_TEMPLATE_APP_ETH_DHCP_H
#define STM32F4_TX_NETX_TEMPLATE_APP_ETH_DHCP_H

/**
 * @brief  创建 DHCP 客户端控制线程
 * @note   由 thread_eth_entry 在 IP/TCP/UDP 启用后调用一次。
 *         调用前必须已 app_eth_config_init
 */
void app_eth_dhcp_create(void);

#endif /* STM32F4_TX_NETX_TEMPLATE_APP_ETH_DHCP_H */

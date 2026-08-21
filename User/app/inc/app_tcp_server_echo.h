/**
 * @file    app_tcp_server_echo.h
 * @brief   TCP server echo：监听 7777，最多 10 个并发 client
 */
#ifndef STM32H7_TX_NETX_TEMPLATE_APP_TCP_SERVER_ECHO_H
#define STM32H7_TX_NETX_TEMPLATE_APP_TCP_SERVER_ECHO_H

/**
 * @brief  创建 TCP server echo 的全部内核对象与线程
 * @note   由 thread_eth_entry 在 IP/TCP 启用后调用一次；真正的 listen
 *         由内部 setup 线程在 link UP 后做
 */
void app_tcp_server_echo_create(void);

#endif /* STM32H7_TX_NETX_TEMPLATE_APP_TCP_SERVER_ECHO_H */

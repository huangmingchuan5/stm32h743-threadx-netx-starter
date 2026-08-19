/**
 * @file    app_tcp_echo.h
 * @brief   TCP echo server：监听 7777，最多 10 个并发 client，每连接一个 worker
 */
#ifndef STM32F4_TX_NETX_TEMPLATE_APP_TCP_ECHO_H
#define STM32F4_TX_NETX_TEMPLATE_APP_TCP_ECHO_H

/**
 * @brief  创建 TCP echo 服务的全部内核对象与线程
 * @note   由 thread_eth_entry 在 IP/TCP 启用后调用一次；真正的 listen
 *         由内部 setup 线程在 link UP 后做
 */
void app_tcp_echo_create(void);

#endif /* STM32F4_TX_NETX_TEMPLATE_APP_TCP_ECHO_H */

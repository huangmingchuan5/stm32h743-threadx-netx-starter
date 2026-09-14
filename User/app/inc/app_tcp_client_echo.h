/**
 * @file    app_tcp_client_echo.h
 * @brief   TCP client echo 配置与创建接口
 */
#ifndef STM32H7_TX_NETX_TEMPLATE_APP_TCP_CLIENT_ECHO_H
#define STM32H7_TX_NETX_TEMPLATE_APP_TCP_CLIENT_ECHO_H

#define APP_TCP_CLIENT_ECHO_SERVER_IP_A     192U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_B     168U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_C     31U
#define APP_TCP_CLIENT_ECHO_SERVER_IP_D     143U
#define APP_TCP_CLIENT_ECHO_SERVER_PORT     7777U

/**
 * @brief  创建 TCP client echo 线程
 * @note   IP 地址就绪后连接配置的服务端，收到数据后原样回发
 */
void app_tcp_client_echo_create(void);

#endif /* STM32H7_TX_NETX_TEMPLATE_APP_TCP_CLIENT_ECHO_H */
/**
 * @file    app_tcp_client_echo.c
 * @brief   NetX Duo TCP client echo
 */
#include "app_tcp_client_echo.h"
#include "app_eth_link_check.h"
#include "nx_api.h"
#include "threadx_printf.h"
#include "tx_api.h"
#include <stdint.h>

#define TCP_CLIENT_ECHO_SERVER_IP                                      \
    IP_ADDRESS(APP_TCP_CLIENT_ECHO_SERVER_IP_A,                        \
               APP_TCP_CLIENT_ECHO_SERVER_IP_B,                        \
               APP_TCP_CLIENT_ECHO_SERVER_IP_C,                        \
               APP_TCP_CLIENT_ECHO_SERVER_IP_D)

#define TCP_CLIENT_ECHO_THREAD_PRIO       5U
#define TCP_CLIENT_ECHO_STACK_SIZE        2048U
#define TCP_CLIENT_ECHO_WINDOW_SIZE       2048U
#define TCP_CLIENT_ECHO_CONNECT_TICKS     (3U * NX_IP_PERIODIC_RATE)
#define TCP_CLIENT_ECHO_IO_TICKS          (3U * NX_IP_PERIODIC_RATE)
#define TCP_CLIENT_ECHO_RETRY_TICKS       (2U * NX_IP_PERIODIC_RATE)

extern NX_IP ip;

static TX_THREAD s_client_thread;
static uint64_t s_client_stack[
    TCP_CLIENT_ECHO_STACK_SIZE / sizeof(uint64_t)];

/**
 * @brief  创建、绑定并连接 TCP client echo socket
 * @param  socket_ptr 待初始化的 socket
 * @return NX_SUCCESS 或 NetX 错误码
 */
static UINT tcp_client_echo_connect(NX_TCP_SOCKET *socket_ptr)
{
    UINT status;

    status = nx_tcp_socket_create(&ip, socket_ptr,
                                  "TCP client echo socket",
                                  NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                  NX_IP_TIME_TO_LIVE,
                                  TCP_CLIENT_ECHO_WINDOW_SIZE,
                                  NX_NULL, NX_NULL);
    if (status != NX_SUCCESS)
        return status;

    status = nx_tcp_client_socket_bind(socket_ptr, NX_ANY_PORT,
                                       TCP_CLIENT_ECHO_CONNECT_TICKS);
    if (status != NX_SUCCESS)
        goto delete_socket;

    status = nx_tcp_client_socket_connect(
        socket_ptr,
        TCP_CLIENT_ECHO_SERVER_IP,
        APP_TCP_CLIENT_ECHO_SERVER_PORT,
        TCP_CLIENT_ECHO_CONNECT_TICKS);
    if (status == NX_SUCCESS)
        return NX_SUCCESS;

    nx_tcp_client_socket_unbind(socket_ptr);

delete_socket:
    nx_tcp_socket_delete(socket_ptr);
    return status;
}

/**
 * @brief  断开并释放 TCP client echo socket
 * @param  socket_ptr 已创建并绑定的 socket
 */
static void tcp_client_echo_close(NX_TCP_SOCKET *socket_ptr)
{
    UINT status;

    status = nx_tcp_socket_disconnect(socket_ptr,
                                      TCP_CLIENT_ECHO_IO_TICKS);
    if (status != NX_SUCCESS &&
        status != NX_NOT_CONNECTED &&
        status != NX_DISCONNECT_FAILED) {
        threadx_printf(Netx_Terminal,
                       "[tcp_client_echo] disconnect failed: 0x%X\r\n",
                       status);
    }

    nx_tcp_client_socket_unbind(socket_ptr);
    nx_tcp_socket_delete(socket_ptr);
}

/**
 * @brief  接收一个 TCP packet 并原样回发
 * @param  socket_ptr 已连接的 socket
 * @return NX_SUCCESS、NX_NO_PACKET 或其他 NetX 错误码
 * @note   send 成功后 packet 归 NetX 管理；send 失败时由本函数释放
 */
static UINT tcp_client_echo_once(NX_TCP_SOCKET *socket_ptr)
{
    NX_PACKET *packet_ptr;
    ULONG length = 0U;
    UINT status;

    status = nx_tcp_socket_receive(socket_ptr, &packet_ptr,
                                   TCP_CLIENT_ECHO_IO_TICKS);
    if (status != NX_SUCCESS)
        return status;

    status = nx_packet_length_get(packet_ptr, &length);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        return status;
    }

    status = nx_tcp_socket_send(socket_ptr, packet_ptr,
                                TCP_CLIENT_ECHO_IO_TICKS);
    if (status != NX_SUCCESS) {
        nx_packet_release(packet_ptr);
        return status;
    }

    threadx_printf(Netx_Terminal,
                   "[tcp_client_echo] echoed %lu bytes\r\n",
                   length);
    return NX_SUCCESS;
}

/**
 * @brief  TCP client echo 线程入口
 * @param  thread_input 未使用
 */
static void tcp_client_echo_entry(ULONG thread_input)
{
    NX_TCP_SOCKET socket;
    ULONG actual_status;
    UINT status;

    (void)thread_input;

    nx_ip_status_check(&ip, NX_IP_ADDRESS_RESOLVED, &actual_status,
                       NX_WAIT_FOREVER);

    while (1) {
        app_eth_link_wait_up(TX_WAIT_FOREVER);

        status = tcp_client_echo_connect(&socket);
        if (status != NX_SUCCESS) {
            threadx_printf(Netx_Terminal,
                           "[tcp_client_echo] connect %u.%u.%u.%u:%u "
                           "failed: 0x%X, retry\r\n",
                           (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_A,
                           (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_B,
                           (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_C,
                           (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_D,
                           (unsigned)APP_TCP_CLIENT_ECHO_SERVER_PORT,
                           status);
            tx_thread_sleep(TCP_CLIENT_ECHO_RETRY_TICKS);
            continue;
        }

        threadx_printf(Netx_Terminal,
                       "[tcp_client_echo] connected to %u.%u.%u.%u:%u\r\n",
                       (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_A,
                       (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_B,
                       (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_C,
                       (unsigned)APP_TCP_CLIENT_ECHO_SERVER_IP_D,
                       (unsigned)APP_TCP_CLIENT_ECHO_SERVER_PORT);

        while (app_eth_link_get_state() == APP_ETH_LINK_UP) {
            status = tcp_client_echo_once(&socket);
            if (status == NX_NO_PACKET)
                continue;
            if (status != NX_SUCCESS) {
                threadx_printf(Netx_Terminal,
                               "[tcp_client_echo] echo failed: 0x%X\r\n",
                               status);
                break;
            }
        }

        tcp_client_echo_close(&socket);
        tx_thread_sleep(TCP_CLIENT_ECHO_RETRY_TICKS);
    }
}

/**
 * @brief  创建 TCP client echo 线程
 */
void app_tcp_client_echo_create(void)
{
    UINT status;

    status = tx_thread_create(&s_client_thread,
                              "App TCP Client Echo",
                              tcp_client_echo_entry,
                              0U,
                              s_client_stack,
                              sizeof(s_client_stack),
                              TCP_CLIENT_ECHO_THREAD_PRIO,
                              TCP_CLIENT_ECHO_THREAD_PRIO,
                              TX_NO_TIME_SLICE,
                              TX_AUTO_START);
    if (status != TX_SUCCESS) {
        threadx_printf(Netx_Terminal,
                       "[tcp_client_echo] thread create failed: 0x%X\r\n",
                       status);
    }
}

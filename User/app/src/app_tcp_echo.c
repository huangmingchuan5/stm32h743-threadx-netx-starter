/**
 * @file    app_tcp_echo.c
 * @brief   TCP echo server，端口 7777，最多 10 个并发 client
 *
 * @note    动态 unlisten/listen 模型：单 listen 槽位 + relisten 接力，
 *          socket 池空就 unlisten 关门让超额 SYN 直接 RST。
 *          worker 与 socket 不绑定，N 个 worker 共用 N 个 socket。
 *          详细模型见 echo_callback_refill_or_close 上方注释
 */
#include "app_tcp_echo.h"
#include "app_eth_link_check.h"
#include "tx_api.h"
#include "nx_api.h"
#include "threadx_printf.h"

#define ECHO_PORT             7777u
#define ECHO_MAX_CLIENTS      10u
#define ECHO_LISTEN_BACKLOG   1u
#define ECHO_WINDOW_SIZE      2048u

#define THREAD_ECHO_PRIO      3u
#define THREAD_ECHO_STACK     2048u

extern NX_IP ip;

static NX_TCP_SOCKET s_sockets[ECHO_MAX_CLIENTS];
static TX_THREAD s_workers[ECHO_MAX_CLIENTS];
static uint64_t s_stacks[ECHO_MAX_CLIENTS][THREAD_ECHO_STACK / 8];
static char s_names[ECHO_MAX_CLIENTS][16];

/*
 * 动态 unlisten/listen 模型。
 *
 * NetXDuo 的多 client TCP server 用"接力棒"模型：同一端口同一时刻只有 1 个
 * socket 处于 LISTEN_STATE 占住 listen_request 槽位，SYN 到达消耗它后由
 * 应用 relisten 顶下一个 socket 上去。
 *
 * 资源约束下（10 个 socket 全连上时 free 池空），传统设计是让槽位悬空，
 * 让超额 SYN 进 NetX 的 listen_queue 排队等 relisten。这导致：
 *   1) client 不知道连不上，看到的是 SYN 没 ACK 然后超时重传；
 *   2) listen_queue 里的陈旧 SYN 在 worker relisten 时被消化，accept 失败
 *      触发 0x38，体验差。
 *
 * 这里改成"动态开关门"：socket 池空就 unlisten 整个 listen_request，
 * 让 NetX 对超额 SYN 直接发 RST（端口未 listen），client 立即得知；
 * worker 归还 socket 时再 listen 重开门。
 *
 * 关键不变式：
 *   s_listen_active == TRUE  ⟺  当前 listen_request 已注册（可能槽位有
 *                                socket 也可能槽位空但有 listen_queue 等
 *                                relisten）
 *
 * 两个互斥操作（都包在 s_listen_mutex 里，调用 NetX listen/unlisten/
 * relisten 期间内部再 get IP mutex，顺序固定：先 s_listen_mutex 再
 * IP mutex，没有反向路径，无死锁风险）：
 *
 *   echo_callback_refill_or_close（callback 触发时调）：
 *       从 free_q 拿 socket -> relisten 顶上槽位；
 *       free_q 空 -> unlisten 关门，s_listen_active = FALSE。
 *
 *   echo_worker_return_socket（worker 还 socket 时调）：
 *       s_listen_active == FALSE -> 用该 socket 调 listen 重开门，
 *           s_listen_active = TRUE；
 *       否则 -> 推 free_q 待用。
 *
 * worker 与 socket 不绑定，N 个 worker 共用 N 个 socket。
 */
static TX_MUTEX s_listen_mutex;
static UCHAR s_listen_active; /* TRUE 表示当前 listen_request 已注册 */
static UCHAR s_refill_in_progress; /* 防 callback 重入 refill 循环 */

static TX_QUEUE s_free_q;
static TX_QUEUE s_work_q;
static ULONG s_free_q_buf[ECHO_MAX_CLIENTS]; /* TX_1_ULONG 槽位 */
static ULONG s_work_q_buf[ECHO_MAX_CLIENTS];

/* listen callback 函数前向声明 */
static void echo_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port);

/**
 * @brief  listen callback 触发的"顶槽位或关门"工作
 * @note   调用前提：listen_request 上的某个 socket 刚被 SYN 消耗，槽位空了。
 *         循环从 free_q 拿 socket 顶槽位；free_q 空就 unlisten 关门。
 *
 *         关于循环：relisten 走 NX_CONNECTION_PENDING 路径时（listen_queue 还
 *         有排队 SYN），NetX 会在 relisten 内部同步再调一次 callback。函数
 *         用 s_refill_in_progress 防重入，nested callback 推完 socket 到
 *         work_q 后直接 return；外层 while 循环负责把下一个 SYN 也消化掉，
 *         直到 listen_queue 排空或 free_q 空。这样避免 nested relisten
 *         持有过期 listen_request 引用的风险
 */
static void echo_callback_refill_or_close(void)
{
    NX_TCP_SOCKET *next_sock;
    UINT status;

    tx_mutex_get(&s_listen_mutex, TX_WAIT_FOREVER);

    if (s_refill_in_progress) {
        /* 重入：外层循环会继续消化 free_q，这里直接退出 */
        tx_mutex_put(&s_listen_mutex);
        return;
    }
    s_refill_in_progress = NX_TRUE;

    while (1) {
        next_sock = NX_NULL;
        status = tx_queue_receive(&s_free_q, &next_sock, TX_NO_WAIT);
        if (status != TX_SUCCESS) {
            /*
             * free_q 空，关门。NetX 会清掉 listen_queue 里所有排队
             * SYN（unlisten 内部行为），新 SYN 一律 RST。
             */
            status = nx_tcp_server_socket_unlisten(&ip, ECHO_PORT);
            if (status == NX_SUCCESS) {
                s_listen_active = NX_FALSE;
                threadx_printf(Netx_Terminal,
                               "[echo] listen closed (pool empty)\r\n");
            } else {
                threadx_printf(Netx_Terminal,
                               "[echo] unlisten failed: 0x%X\r\n",
                               status);
            }
            break;
        }

        status = nx_tcp_server_socket_relisten(&ip, ECHO_PORT, next_sock);
        if (status == NX_SUCCESS) {
            /* 顶上了，listen_queue 排空，正常等下次 SYN */
            break;
        }
        if (status == NX_CONNECTION_PENDING) {
            /*
             * listen_queue 还有 SYN 被立即消化，nested callback
             * 已经把对应 socket 推 work_q 了。继续循环，看是不是
             * 还要再 relisten 顶下一个 SYN。
             */
            continue;
        }
        /* 其他错误极少见，归还 socket 避免泄漏并退出 */
        threadx_printf(Netx_Terminal,
                       "[echo] cb relisten failed: 0x%X\r\n", status);
        tx_queue_send(&s_free_q, &next_sock, TX_NO_WAIT);
        break;
    }

    s_refill_in_progress = NX_FALSE;
    tx_mutex_put(&s_listen_mutex);
}

/**
 * @brief  worker 完成 echo cleanup 后归还 socket
 * @param  sock 待归还的 socket
 * @note   当前关门：用该 socket 调 listen 重开门；
 *         当前开门：简单推 free_q 待用
 */
static void echo_worker_return_socket(NX_TCP_SOCKET *sock)
{
    UINT status;

    tx_mutex_get(&s_listen_mutex, TX_WAIT_FOREVER);

    if (!s_listen_active) {
        status = nx_tcp_server_socket_listen(&ip, ECHO_PORT, sock,
                                             ECHO_LISTEN_BACKLOG,
                                             echo_listen_callback);
        if (status == NX_SUCCESS) {
            s_listen_active = NX_TRUE;
            threadx_printf(Netx_Terminal,
                           "[echo] listen reopened\r\n");
        } else {
            threadx_printf(Netx_Terminal,
                           "[echo] reopen listen failed: 0x%X\r\n",
                           status);
            /* 退路：推 free_q 等下次再试 */
            tx_queue_send(&s_free_q, &sock, TX_NO_WAIT);
        }
    } else {
        tx_queue_send(&s_free_q, &sock, TX_NO_WAIT);
    }

    tx_mutex_put(&s_listen_mutex);
}

/**
 * @brief  NetX listen callback：SYN 到达消耗 listen 槽位时被调
 * @param  socket_ptr 刚被 SYN 消耗的 socket，应用要负责对它 accept
 * @param  port       监听端口（未使用）
 * @note   把 socket 派给 worker 去 accept，并触发 refill/关门维护槽位
 */
static void echo_listen_callback(NX_TCP_SOCKET *socket_ptr, UINT port)
{
    (void)port;

    /* 把被消耗的 socket 派给 worker */
    tx_queue_send(&s_work_q, &socket_ptr, TX_NO_WAIT);

    /* 顶槽位或关门 */
    echo_callback_refill_or_close();
}

/**
 * @brief  收尾断开 + unaccept，让 socket 可以重新进入 LISTEN 池
 * @param  sock 要清理的 socket
 * @note   disconnect 死等：等 FIN 被 ACK / RST / NetX 重传耗尽。
 *         NX_NOT_CONNECTED (0x38)：对端 RST 后 NetX 已退出 ESTABLISHED；
 *         NX_DISCONNECT_FAILED (0x41)：disconnect 挂起期间被 RST 异步唤醒。
 *         按 NetXDuo 官方 demo idiom 这两种状态都吞掉继续 unaccept
 */
static void echo_disconnect_cleanup(NX_TCP_SOCKET *sock)
{
    UINT status;

    /*
     * disconnect 死等：等 FIN 被 ACK / RST 到来 / NetX 重传耗尽。
     *
     * NX_NOT_CONNECTED (0x38) 是预期路径：对端发 RST 后 NetX 已经把
     * socket 推出 ESTABLISHED 态，再调 disconnect 就会返这个。
     * NX_DISCONNECT_FAILED (0x41) 同理：disconnect 挂起期间被 RST
     * 异步唤醒。按 NetXDuo 官方 demo idiom 都吞掉继续 unaccept。
     */
    status = nx_tcp_socket_disconnect(sock, TX_WAIT_FOREVER);
    if (status != NX_SUCCESS &&
        status != NX_NOT_CONNECTED &&
        status != NX_DISCONNECT_FAILED)
        threadx_printf(Netx_Terminal,
                       "[echo] disconnect failed: 0x%X\r\n", status);

    nx_tcp_server_socket_unaccept(sock);
}

/**
 * @brief  echo worker 线程入口
 * @param  idx 线程编号，仅用于命名调试（不与 socket 绑定）
 * @note   循环：从 work_q 拿 socket，accept，零拷贝 echo 直到对端断开，
 *         cleanup 后归还 socket。N 个 worker 共用 N 个 socket，不绑定
 */
static void echo_worker_entry(ULONG idx)
{
    NX_TCP_SOCKET *sock;
    NX_PACKET *pkt;
    UINT status;

    (void)idx;

    while (1) {
        status = tx_queue_receive(&s_work_q, &sock, TX_WAIT_FOREVER);
        if (status != TX_SUCCESS)
            continue;

        /*
         * socket 已在 SYN_RECEIVED：NetX 收到 SYN 时已 send 出 SYN+ACK，
         * accept 在这里等三次握手完成。
         */
        status = nx_tcp_server_socket_accept(sock, TX_WAIT_FOREVER);
        if (status != NX_SUCCESS) {
            threadx_printf(Netx_Terminal,
                           "[echo] accept failed: 0x%X\r\n", status);
            echo_disconnect_cleanup(sock);
            echo_worker_return_socket(sock);
            continue;
        }

        {
            ULONG peer_ip = 0, peer_port = 0;

            nx_tcp_socket_peer_info_get(sock, &peer_ip, &peer_port);
            threadx_printf(Netx_Terminal,
                           "[echo] client connected on socket %p"
                           " from %lu.%lu.%lu.%lu:%lu\r\n",
                           (void *)sock,
                           (peer_ip >> 24) & 0xFF,
                           (peer_ip >> 16) & 0xFF,
                           (peer_ip >> 8) & 0xFF,
                           peer_ip & 0xFF,
                           peer_port);
        }

        /*
         * 零拷贝 echo：收到的 NX_PACKET 直接 send 回去。
         *   - send 成功后包归 NetX 管理，禁止再 release。
         *   - send 失败必须自己 release，否则泄漏包池。
         *
         * send 用死等：有限超时会在 TX 队列短暂拥塞或 delayed ACK 时
         * 错杀仍然健康的连接，触发反复 SYN/FIN/RST 冲击包池。死等下
         * send 失败意味着连接真挂了，cleanup 路径正确。
         */
        while (1) {
            status = nx_tcp_socket_receive(sock, &pkt, NX_WAIT_FOREVER);
            if (status != NX_SUCCESS) {
                threadx_printf(Netx_Terminal,
                               "[echo] receive failed: 0x%X\r\n",
                               status);
                break;
            }

            status = nx_tcp_socket_send(sock, pkt, TX_WAIT_FOREVER);
            if (status != NX_SUCCESS) {
                threadx_printf(Netx_Terminal,
                               "[echo] send failed: 0x%X\r\n",
                               status);
                nx_packet_release(pkt);
                break;
            }
        }

        threadx_printf(Netx_Terminal,
                       "[echo] client disconnected on socket %p\r\n",
                       (void *)sock);

        echo_disconnect_cleanup(sock);
        echo_worker_return_socket(sock);
    }
}

static TX_THREAD s_setup_thread;
static uint64_t s_setup_stack[1024 / 8];

/**
 * @brief  echo 服务首次启动：等 link UP 后创建 socket 并首次 listen
 * @param  arg 未使用
 * @note   独立线程是为了不阻塞 thread_eth_entry——后者还要在它后面拉
 *         其他业务模块
 */
static void echo_setup_entry(ULONG arg)
{
    NX_TCP_SOCKET *sock;
    ULONG i;
    UINT status;

    (void)arg;

    app_eth_link_wait_up(TX_WAIT_FOREVER);

    for (i = 0; i < ECHO_MAX_CLIENTS; i++) {
        status = nx_tcp_socket_create(&ip, &s_sockets[i], "echo socket",
                                      NX_IP_NORMAL, NX_FRAGMENT_OKAY,
                                      NX_IP_TIME_TO_LIVE,
                                      ECHO_WINDOW_SIZE, NX_NULL, NX_NULL);
        if (status != NX_SUCCESS) {
            threadx_printf(Netx_Terminal,
                           "[echo] socket %lu create failed: 0x%X\r\n",
                           i, status);
            return;
        }
    }

    /* socket[0] 占住 listen 槽位首次开门，其余进 free_q */
    tx_mutex_get(&s_listen_mutex, TX_WAIT_FOREVER);
    status = nx_tcp_server_socket_listen(&ip, ECHO_PORT, &s_sockets[0],
                                         ECHO_LISTEN_BACKLOG,
                                         echo_listen_callback);
    if (status != NX_SUCCESS) {
        tx_mutex_put(&s_listen_mutex);
        threadx_printf(Netx_Terminal,
                       "[echo] initial listen failed: 0x%X\r\n", status);
        return;
    }
    s_listen_active = NX_TRUE;
    tx_mutex_put(&s_listen_mutex);

    for (i = 1; i < ECHO_MAX_CLIENTS; i++) {
        sock = &s_sockets[i];
        tx_queue_send(&s_free_q, &sock, TX_NO_WAIT);
    }

    threadx_printf(Netx_Terminal,
                   "[echo] listen on port %u, max clients %u\r\n",
                   (unsigned)ECHO_PORT, (unsigned)ECHO_MAX_CLIENTS);

    while (1) {
        tx_thread_sleep(10000);
    }
}

/**
 * @brief  创建 TCP echo 服务所需的全部内核对象与线程
 * @note   thread_eth_entry 在 IP/TCP 启用后调用一次。
 *         监听端口 7777，最大并发 ECHO_MAX_CLIENTS 个 client。
 *         真正的 listen 由 echo_setup_entry 在 link UP 后做，本函数立即返回
 */
void app_tcp_echo_create(void)
{
    ULONG i;

    tx_mutex_create(&s_listen_mutex, "echo listen mtx", TX_NO_INHERIT);
    s_listen_active = NX_FALSE;
    s_refill_in_progress = NX_FALSE;

    tx_queue_create(&s_free_q, "echo free q", TX_1_ULONG,
                    s_free_q_buf, sizeof(s_free_q_buf));
    tx_queue_create(&s_work_q, "echo work q", TX_1_ULONG,
                    s_work_q_buf, sizeof(s_work_q_buf));

    for (i = 0; i < ECHO_MAX_CLIENTS; i++) {
        snprintf(s_names[i], sizeof(s_names[i]), "echo#%lu", i);

        tx_thread_create(&s_workers[i], /* 任务控制块地址 */
                         s_names[i], /* 任务名 */
                         echo_worker_entry, /* 启动任务函数地址 */
                         i, /* 传递给任务的参数 */
                         &s_stacks[i][0], /* 堆栈基地址 */
                         THREAD_ECHO_STACK, /* 堆栈空间大小 */
                         THREAD_ECHO_PRIO, /* 任务优先级 */
                         THREAD_ECHO_PRIO, /* 任务抢占阀值 */
                         TX_NO_TIME_SLICE, /* 不开启时间片 */
                         TX_AUTO_START); /* 创建后立即启动 */
    }

    tx_thread_create(&s_setup_thread, /* 任务控制块地址 */
                     "echo setup", /* 任务名 */
                     echo_setup_entry, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &s_setup_stack[0], /* 堆栈基地址 */
                     sizeof(s_setup_stack), /* 堆栈空间大小 */
                     THREAD_ECHO_PRIO, /* 任务优先级 */
                     THREAD_ECHO_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */
}

/**
 * @file    app_eth_link_check.c
 * @brief   link 状态监测线程 + RX 描述符 buffer 兜底补给
 *
 * @note    周期查 PHY，按上下沿调 NX_LINK_ENABLE/DISABLE 维护应用层
 *          driver_enabled 标志（不依赖 ip.nx_ip_driver_link_up）。
 *          link UP 时顺便兜底补给 RX 描述符 buffer，避免压力下 DMA 死锁
 */
#include "app_eth_link_check.h"
#include "tx_api.h"
#include "nx_api.h"
#include "nx_stm32_eth_config.h"
#include "threadx_printf.h"

#define THREAD_ETH_LINK_CHECK_PRIO       10u
#define THREAD_ETH_LINK_CHECK_STACK_SIZE 2048u

static TX_THREAD thread_eth_link_check_tcb;
static uint64_t thread_eth_link_check_stack[
    THREAD_ETH_LINK_CHECK_STACK_SIZE / 8];

/*
 * 内部事件标志，纯电平语义：
 *   EVT_LINK_UP   当前 UP 时置位，DOWN 时清零
 *   EVT_LINK_DOWN 当前 DOWN 时置位，UP 时清零
 * wait_up / wait_down / wait_change 都基于这两个 bit，多个 waiter 用
 * TX_AND（不清除）共享，互不抢事件，也不会丢事件——电平是状态的真实映射。
 */
#define EVT_LINK_UP     (1UL << 0)
#define EVT_LINK_DOWN   (1UL << 1)

static TX_EVENT_FLAGS_GROUP s_link_events;

static volatile enum app_eth_link_state s_link_state = APP_ETH_LINK_DOWN;

static void thread_eth_link_check_entry(ULONG thread_input);

/**
 * @brief  创建 link 状态监测线程
 * @note   thread_eth_entry 在 nx_ip_create 之后调用一次。
 *         事件初值置 DOWN，订阅者初次进入时不会因历史事件假触发
 */
void app_eth_link_check_create(void)
{
    tx_event_flags_create(&s_link_events, "eth link events");

    /* 初始视为 DOWN：DOWN 电平置位，UP 清零 */
    tx_event_flags_set(&s_link_events, EVT_LINK_DOWN, TX_OR);

    tx_thread_create(&thread_eth_link_check_tcb, /* 任务控制块地址 */
                     "App ETH Link Check", /* 任务名 */
                     thread_eth_link_check_entry, /* 启动任务函数地址 */
                     0, /* 传递给任务的参数 */
                     &thread_eth_link_check_stack[0], /* 堆栈基地址 */
                     THREAD_ETH_LINK_CHECK_STACK_SIZE, /* 堆栈空间大小 */
                     THREAD_ETH_LINK_CHECK_PRIO, /* 任务优先级 */
                     THREAD_ETH_LINK_CHECK_PRIO, /* 任务抢占阀值 */
                     TX_NO_TIME_SLICE, /* 不开启时间片 */
                     TX_AUTO_START); /* 创建后立即启动 */
}

/**
 * @brief  非阻塞查询当前 link 状态
 * @return APP_ETH_LINK_UP / APP_ETH_LINK_DOWN
 */
enum app_eth_link_state app_eth_link_get_state(void)
{
    return s_link_state;
}

/**
 * @brief  阻塞等 link 进入 UP（电平语义）
 * @param  wait_ticks 等待 tick 数，TX_WAIT_FOREVER 永久等待
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）
 * @note   进入时若已是 UP，立即返回 TX_SUCCESS
 */
UINT app_eth_link_wait_up(ULONG wait_ticks)
{
    ULONG actual;

    return tx_event_flags_get(&s_link_events, EVT_LINK_UP, TX_AND,
                              &actual, wait_ticks);
}

/**
 * @brief  阻塞等 link 进入 DOWN（电平语义）
 * @param  wait_ticks 等待 tick 数，TX_WAIT_FOREVER 永久等待
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）
 * @note   进入时若已是 DOWN，立即返回 TX_SUCCESS
 */
UINT app_eth_link_wait_down(ULONG wait_ticks)
{
    ULONG actual;

    return tx_event_flags_get(&s_link_events, EVT_LINK_DOWN, TX_AND,
                              &actual, wait_ticks);
}

/**
 * @brief  初始化订阅者，把它同步到当前 link 状态
 * @param  sub 调用方持有的订阅句柄
 * @note   调完后首次 wait_change 等的是"对立面"，不会因线程启动前的历史变化
 *         立刻返回。想"先处理一次当前状态"的应用，自行调 app_eth_link_get_state
 */
void app_eth_link_sub_init(struct link_sub *sub)
{
    /* 同步到当前状态，下次 wait_change 等的是"对立面" */
    sub->last_state = s_link_state;
}

/**
 * @brief  阻塞等订阅者所记录状态发生变化
 * @param  sub        订阅句柄，函数内会更新其 last_state
 * @param  out_state  成功时写入最新状态；可为 NX_NULL
 * @param  wait_ticks 等待 tick 数
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）/ 其他 NetX 错误
 * @note   多线程可各自持有自己的 sub 并行等待，互不抢事件。
 *         link 抖回原状态时，返回的"最新状态"可能与 last_state 相同——这是
 *         真实情况，应用层自行判断是否忽略抖动
 */
UINT app_eth_link_wait_change(struct link_sub *sub,
                              enum app_eth_link_state *out_state,
                              ULONG wait_ticks)
{
    ULONG actual;
    ULONG target_flag;
    UINT status;

    /*
     * "变化"被定义为"进入和上次不同的状态"，于是退化成一次电平等待——
     * 等"对立"那一边的 EVT bit 置位。电平 bit 是状态的真实映射，
     * 不存在丢事件或竞态。多个订阅者各自等各自的 bit，互不干扰。
     */
    target_flag = (sub->last_state == APP_ETH_LINK_UP) ?
                      EVT_LINK_DOWN :
                      EVT_LINK_UP;

    status = tx_event_flags_get(&s_link_events, target_flag, TX_AND,
                                &actual, wait_ticks);
    if (status != TX_SUCCESS)
        return status;

    /*
     * 取一次当前真实状态返回。极少数情况下 link 在 wait 返回后又抖回原状态，
     * 此时返回的可能是"和 last_state 相同"——这是真实情况，把它如实告诉应用，
     * 应用可自行判断是否要忽略抖动。
     */
    sub->last_state = s_link_state;
    if (out_state != NX_NULL)
        *out_state = sub->last_state;

    return NX_SUCCESS;
}

/**
 * @brief  在 link_check 线程上下文中更新 link 状态
 * @param  new_state 新状态
 * @note   纯电平语义：先清对侧 bit 再置本侧 bit。所有 waiter（wait_up /
 *         wait_down / wait_change）都基于这两个 bit，无脉冲、无 version，
 *         没有任何竞态窗口
 */
static void link_state_update(enum app_eth_link_state new_state)
{
    if (new_state == s_link_state)
        return;

    s_link_state = new_state;

    if (new_state == APP_ETH_LINK_UP) {
        tx_event_flags_set(&s_link_events, ~EVT_LINK_DOWN, TX_AND);
        tx_event_flags_set(&s_link_events, EVT_LINK_UP, TX_OR);
    } else {
        tx_event_flags_set(&s_link_events, ~EVT_LINK_UP, TX_AND);
        tx_event_flags_set(&s_link_events, EVT_LINK_DOWN, TX_OR);
    }
}

extern NX_IP ip;


/**
 * @brief  link UP 周期补 RX 描述符 buffer，避免包池压力下死锁
 * @note   背景：STM32 HAL ETH 用 HAL_ETH_RxAllocateCallback 给 RX 描述符要
 *         buffer，压力大时包池空 alloc 失败，HAL 只在下次 HAL_ETH_ReadData
 *         才会重试；4 个描述符同时空了 DMA 无 buffer 可写，所有新包直接丢，
 *         ACK 也进不来，TCP 重传到上限 connection_reset，全部 socket 一起断。
 *
 *         实现复用 H7 HAL ETH_UpdateDescriptor 核心逻辑：
 *         遍历 RxBuildDescCnt 个待补描述符，BackupAddr0==0 的就调
 *         HAL_ETH_RxAllocateCallback 补 buffer，再写 DESC0 和 DESC3，
 *         最后更新 DMA tail pointer。
 *
 *         互斥：借用 NetX 自己的 ip_protection mutex 与 IP 线程串行——IP 线程
 *         deferred packet 路径会进 HAL_ETH_ReadData 改这些字段。关中断保护
 *         "写 BackupAddr0 + 写 DESC2"这对操作不被中断撕裂
 */
static void link_check_rx_refill(void)
{
    ETH_DMADescTypeDef *dmarxdesc;
    uint32_t descidx;
    uint32_t desccount;

    tx_mutex_get(&ip.nx_ip_protection, TX_WAIT_FOREVER);

    descidx = heth.RxDescList.RxBuildDescIdx;
    dmarxdesc = (ETH_DMADescTypeDef *)heth.RxDescList.RxDesc[descidx];
    desccount = heth.RxDescList.RxBuildDescCnt;

    while (desccount > 0U) {
        if (READ_REG(dmarxdesc->BackupAddr0) == 0U) {
            uint8_t *buff = NULL;

            HAL_ETH_RxAllocateCallback(&buff);
            if (buff == NULL)
                break; /* 包池仍空，下次再试 */
            /* BackupAddr0 和 DESC0 必须作为同一次 buffer 挂载完成。 */
            __disable_irq();
            WRITE_REG(dmarxdesc->BackupAddr0, (uint32_t)buff);
            WRITE_REG(dmarxdesc->DESC0, (uint32_t)buff);
            __enable_irq();
        } else {
            WRITE_REG(dmarxdesc->DESC0,
                      (uint32_t)dmarxdesc->BackupAddr0);
        }

        if (heth.RxDescList.ItMode != 0U) {
            WRITE_REG(dmarxdesc->DESC3,
                      ETH_DMARXNDESCRF_OWN |
                      ETH_DMARXNDESCRF_BUF1V |
                      ETH_DMARXNDESCRF_IOC);
        } else {
            WRITE_REG(dmarxdesc->DESC3,
                      ETH_DMARXNDESCRF_OWN |
                      ETH_DMARXNDESCRF_BUF1V);
        }

        /* 等价 INCR_RX_DESC_INDEX */
        descidx = (descidx + 1U) % ETH_RX_DESC_CNT;
        dmarxdesc = (ETH_DMADescTypeDef *)heth.RxDescList.RxDesc[descidx];
        desccount--;
    }

    if (heth.RxDescList.RxBuildDescCnt != desccount) {
        /* 至少补了一个，更新 DMA tail pointer */
        uint32_t tailidx = (ETH_RX_DESC_CNT + descidx - 1U) %
                           ETH_RX_DESC_CNT;
        __DMB();
        WRITE_REG(heth.Instance->DMACRDTPR,
                  (uint32_t)(heth.Init.RxDesc + tailidx));

        heth.RxDescList.RxBuildDescIdx = descidx;
        heth.RxDescList.RxBuildDescCnt = desccount;
    }

    tx_mutex_put(&ip.nx_ip_protection);
}


#define LINK_CHECK_INTERVAL (NX_IP_PERIODIC_RATE)

/**
 * @brief  link 状态监测线程入口
 * @param  thread_input 未使用
 * @note   等 IP 实例 INITIALIZE_DONE 后周期（NX_IP_PERIODIC_RATE）做：
 *         1) 查 PHY 状态，按上下沿调 NX_LINK_ENABLE / NX_LINK_DISABLE，
 *            维护应用层 driver_enabled 标志（不依赖 ip.nx_ip_driver_link_up）；
 *         2) link UP 时调 link_check_rx_refill 兜底补 RX 描述符 buffer
 */
static void thread_eth_link_check_entry(ULONG thread_input)
{
    ULONG actual_status;
    ULONG return_value;
    UINT status;
    UCHAR phy_link_up;
    /*
     * 应用层自己维护"驱动是否成功 enable 过"的标志。
     * 不依赖 ip.nx_ip_driver_link_up——NetX IP 线程在调驱动之前就把它预设为
     * NX_TRUE 了（nx_ip_thread_entry.c:151），enable 失败也不会回滚。
     */
    UCHAR driver_enabled = NX_FALSE;

    (void)thread_input;

    /* 等待 IP 实例初始化完成 */
    nx_ip_status_check(&ip, NX_IP_INITIALIZE_DONE, &actual_status,
                       NX_WAIT_FOREVER);

    while (1) {
        /*
         * 查询 PHY 当前 link 状态（不阻塞）。该 API 内部会发
         * NX_LINK_GET_STATUS 命令给驱动，驱动通过
         * nx_eth_phy_get_link_state() 读 PHY 寄存器返回真实状态。
         */
        status = nx_ip_interface_status_check(&ip, 0,
                                              NX_IP_LINK_ENABLED,
                                              &actual_status,
                                              NX_NO_WAIT);
        phy_link_up = (status == NX_SUCCESS) ? NX_TRUE : NX_FALSE;

        if (phy_link_up == NX_TRUE && driver_enabled == NX_FALSE) {
            status = nx_ip_driver_interface_direct_command(&ip,
                NX_LINK_ENABLE, 0, &return_value);
            if (status == NX_SUCCESS) {
                driver_enabled = NX_TRUE;
                link_state_update(APP_ETH_LINK_UP);
                threadx_printf(Netx_Terminal,
                               "[link_check] ETH link up.\r\n");
            } else if (status == NX_ALREADY_ENABLED) {
                driver_enabled = NX_TRUE;
                link_state_update(APP_ETH_LINK_UP);
                threadx_printf(Netx_Terminal,
                               "[link_check] ETH already link up.\r\n");
            } else {
                threadx_printf(Netx_Terminal,
                               "[link_check] ETH link enable failed: 0x%X\r\n",
                               status);
            }
        } else if (phy_link_up == NX_FALSE && driver_enabled == NX_TRUE) {
            status = nx_ip_driver_interface_direct_command(&ip,
                NX_LINK_DISABLE, 0, &return_value);
            if (status == NX_SUCCESS) {
                driver_enabled = NX_FALSE;
                nx_arp_dynamic_entries_invalidate(&ip);
                link_state_update(APP_ETH_LINK_DOWN);
                threadx_printf(Netx_Terminal,
                               "[link_check] ETH link down.\r\n");
            } else {
                threadx_printf(Netx_Terminal,
                               "[link_check] ETH link disable failed: 0x%X\r\n",
                               status);
            }
        }

        /*
         * link UP 时定期补 RX buffer：HAL_ETH_RxAllocateCallback 之前
         * alloc 失败留下的空描述符，趁包池现在有包时补齐，否则 DMA
         * 没 buffer 可写，所有新包都丢，连锁触发 TCP 重传/连接 reset。
         * link DOWN 时不必补：DMA 已停，描述符塞 buffer 也没意义，反而
         * 占用本就紧张的包池。
         */
        if (driver_enabled == NX_TRUE)
            link_check_rx_refill();

        tx_thread_sleep(LINK_CHECK_INTERVAL);
    }
}

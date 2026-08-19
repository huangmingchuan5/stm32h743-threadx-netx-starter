/**
 * @file    app_eth_link_check.h
 * @brief   link 状态监测与订阅 API
 *
 * @par Public API
 *      - app_eth_link_check_create    创建监测线程，由 thread_eth_entry 调一次
 *      - app_eth_link_get_state       非阻塞查当前 link 状态
 *      - app_eth_link_wait_up         阻塞等 link UP（电平语义）
 *      - app_eth_link_wait_down       阻塞等 link DOWN（电平语义）
 *      - app_eth_link_sub_init        初始化订阅句柄
 *      - app_eth_link_wait_change     阻塞等 link 发生新变化
 *
 * @note    电平语义而非脉冲：状态进入 UP/DOWN 时事件 bit 长置位，所以
 *          wait_up/wait_down 不会丢事件；多个线程持自己的 sub 并行
 *          wait_change，互不抢事件
 */
#ifndef STM32F4_TX_NETX_TEMPLATE_APP_ETH_LINK_CHECK_H
#define STM32F4_TX_NETX_TEMPLATE_APP_ETH_LINK_CHECK_H

#include "tx_api.h"

enum app_eth_link_state {
    APP_ETH_LINK_DOWN = 0,
    APP_ETH_LINK_UP = 1,
};

/**
 * @brief  link 变化订阅句柄
 * @note   每个等 wait_change 的线程各自持有一个实例。字段为实现细节，
 *         外部只需声明实例并把地址传给相关 API
 */
struct link_sub {
    enum app_eth_link_state last_state;
};

/**
 * @brief  创建 link 监测线程
 * @note   由 thread_eth_entry 在 nx_ip_create 之后调一次
 */
void app_eth_link_check_create(void);

/**
 * @brief  非阻塞查询当前 link 状态
 * @return APP_ETH_LINK_UP / APP_ETH_LINK_DOWN
 */
enum app_eth_link_state app_eth_link_get_state(void);

/**
 * @brief  阻塞等 link 进入 UP
 * @param  wait_ticks 等待 tick 数，TX_WAIT_FOREVER 永久等待
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）
 * @note   进入时若已是 UP 立即返回 TX_SUCCESS
 */
UINT app_eth_link_wait_up(ULONG wait_ticks);

/**
 * @brief  阻塞等 link 进入 DOWN
 * @param  wait_ticks 等待 tick 数，TX_WAIT_FOREVER 永久等待
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）
 * @note   进入时若已是 DOWN 立即返回 TX_SUCCESS
 */
UINT app_eth_link_wait_down(ULONG wait_ticks);

/**
 * @brief  初始化订阅句柄，同步到当前 link 状态
 * @param  sub 调用方持有的订阅句柄
 * @note   首次 wait_change 等的是"对立面"，不会因线程启动前的历史变化误触发
 */
void app_eth_link_sub_init(struct link_sub *sub);

/**
 * @brief  阻塞等订阅者记录的状态发生变化
 * @param  sub        订阅句柄，函数内部会更新其 last_state
 * @param  out_state  成功时写入最新状态；可为 NX_NULL
 * @param  wait_ticks 等待 tick 数
 * @return TX_SUCCESS / TX_NO_EVENTS（超时）
 * @note   多线程可各持 sub 并行等待。link 抖回原状态时返回值有可能与
 *         上次相同——这是真实情况，应用层自行决定是否忽略抖动
 */
UINT app_eth_link_wait_change(struct link_sub *sub,
                              enum app_eth_link_state *out_state,
                              ULONG wait_ticks);

#endif /* STM32F4_TX_NETX_TEMPLATE_APP_ETH_LINK_CHECK_H */

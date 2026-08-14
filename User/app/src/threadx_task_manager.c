/**
 * @file    threadx_task_manager.c
 * @brief   业务任务集中创建入口
 *
 * @note    由 AppTaskStart 在 OS 适配完成后调用一次。新增模块时把它的
 *          *_create() 添加到 AppTaskCreate 内，统一在这里启动业务线程
 */
#include "threadx_task_manager.h"

/**
 * @brief  集中创建本工程所有业务任务
 * @note   thread_eth_create 通过 extern 引用，避免在公共头里二次声明
 */
void AppTaskCreate(void)
{
}
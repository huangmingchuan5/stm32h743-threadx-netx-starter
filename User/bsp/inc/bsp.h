/**
 * @file    bsp.h
 * @brief   板级支持初始化入口
 */
#ifndef __BSP_H
#define __BSP_H

/**
 * @brief  系统初始化（HAL + 时钟 + RTT 通道）
 * @note   main 中在进入内核之前调用一次
 */
void System_Init(void);

/**
 * @brief  外设、硬件初始化，在 ThreadX 启动任务中调用
 * @note   需要操作系统已就绪（如使用 tx_thread_sleep 的硬件初始化要放这里）
 */
void bsp_Init(void);

#endif //__BSP_H
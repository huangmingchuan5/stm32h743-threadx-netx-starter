/**
 * @file    bsp_RTT_viewer.h
 * @brief   SEGGER RTT viewer 初始化入口
 */
#ifndef __BSP_RTT_VIEWER_H
#define __BSP_RTT_VIEWER_H

/**
 * @brief  RTT 通道 0 上下行配置 + 清空 16 个终端
 * @note   由 System_Init 调用
 */
void RTT_viewer_init(void);

#endif //__BSP_RTT_VIEWER_H
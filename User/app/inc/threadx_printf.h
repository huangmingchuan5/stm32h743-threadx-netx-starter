/**
 * @file    threadx_printf.h
 * @brief   ThreadX 适配的多终端 printf 对外接口
 *
 * @note    通过 SEGGER RTT 把不同业务的输出分流到独立终端，调用前
 *          必须已调 threadx_printf_mutex_init（由启动任务负责）
 */
#ifndef __THREADX_PRINTF_H
#define __THREADX_PRINTF_H

#include "SEGGER_RTT.h"

/**
 * @brief  RTT 终端分配表
 * @note   编号从 1 开始留出 0 给标准 RTT，未列入此枚举的终端号也可用，
 *         但建议沿用本表保持模块统一
 */
typedef enum {
    Manage_Terminal = 1, /* ThreadX 任务状态 / CPU 利用率 */
    Net_Info_Terminal, /* 以太网状态/统计表 */
    Netx_Terminal, /* NetX 业务日志（dhcp/echo/link 等） */
    Other_Terminal /* 标准 printf / _write 默认终端 */
} Printf_Terminals;

/**
 * @brief  线程安全的多终端 printf
 * @param  terminal 目标终端编号
 * @param  fmt      格式串
 * @param  ...      可变参数
 * @note   单次输出最长 200 字节，超出静默截断
 */
void threadx_printf(Printf_Terminals terminal, const char *fmt, ...);

/**
 * @brief  清空指定终端的内容并复位颜色属性
 * @param  terminal 目标终端编号
 */
void threadx_printf_clear(Printf_Terminals terminal);

/**
 * @brief  向指定终端发送 RTT 控制指令（颜色 / 格式等）
 * @param  terminal 目标终端编号
 * @param  cmd      终端控制指令字符串，可用常量见 SEGGER_RTT.h
 */
void threadx_printf_cmd(Printf_Terminals terminal, const char *cmd);


#endif //__THREADX_PRINTF_H
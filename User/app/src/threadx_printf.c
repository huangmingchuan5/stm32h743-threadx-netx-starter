/**
 * @file    threadx_printf.c
 * @brief   ThreadX 适配的多终端 printf：互斥锁串行化 + SEGGER RTT 分终端输出
 */

#include "threadx_printf.h"
#include "tx_api.h"
#include <stdio.h>

static TX_MUTEX AppPrintfSemp; /* 串行化所有 RTT 写入路径 */

/**
 * @brief  初始化 threadx_printf 内部互斥锁
 * @note   在 ThreadX 启动任务里调用，所有 threadx_printf* 调用前必须已就绪
 */
void threadx_printf_mutex_init(void)
{
    tx_mutex_create(&AppPrintfSemp, "AppPrintfSemp", TX_INHERIT);
}

/**
 * @brief  线程安全的多终端 printf
 * @param  terminal 目标终端编号
 * @param  fmt      格式串
 * @param  ...      可变参数
 * @note   单次输出最长 200 字节，超出会被静默截断。
 *         使用前必须已调用 threadx_printf_mutex_init
 */
void threadx_printf(Printf_Terminals terminal, const char *fmt, ...)
{
    char buf_str[200 + 1]; /* 特别注意，如果printf的变量较多，注意此局部变量的大小是否够用 */
    va_list v_args;

    va_start(v_args, fmt);
    (void)vsnprintf((char *)&buf_str[0], (size_t)sizeof(buf_str),
                    (char const *)fmt, v_args);
    va_end(v_args);

    /* 互斥操作 */
    tx_mutex_get(&AppPrintfSemp, TX_WAIT_FOREVER);

    SEGGER_RTT_SetTerminal(terminal);
    SEGGER_RTT_WriteString(0, buf_str);

    tx_mutex_put(&AppPrintfSemp);
}

/**
 * @brief  清空指定终端的内容并复位颜色属性
 * @param  terminal 目标终端编号
 */
void threadx_printf_clear(Printf_Terminals terminal)
{
    /* 互斥操作 */
    tx_mutex_get(&AppPrintfSemp, TX_WAIT_FOREVER);

    SEGGER_RTT_SetTerminal(terminal);
    SEGGER_RTT_WriteString(0, RTT_CTRL_CLEAR);
    SEGGER_RTT_WriteString(0, RTT_CTRL_RESET);

    tx_mutex_put(&AppPrintfSemp);
}

/**
 * @brief  向指定终端发送 RTT 控制指令（颜色 / 格式等）
 * @param  terminal 目标终端编号
 * @param  cmd      终端控制指令字符串，可用常量见 SEGGER_RTT.h 中的
 *                  RTT_CTRL_TEXT_* / RTT_CTRL_BG_* / RTT_CTRL_RESET 等
 */
void threadx_printf_cmd(Printf_Terminals terminal, const char *cmd)
{
    /* 互斥操作 */
    tx_mutex_get(&AppPrintfSemp, TX_WAIT_FOREVER);

    SEGGER_RTT_SetTerminal(terminal);
    SEGGER_RTT_WriteString(0, cmd);

    tx_mutex_put(&AppPrintfSemp);
}

/**
 * @brief  syscalls 的 _write 弱符号实现，把 printf/puts 输出导到 RTT
 * @param  file 文件描述符（未使用）
 * @param  ptr  数据起始指针
 * @param  len  数据长度
 * @return 写入字节数（恒等于 len）
 * @note   输出固定走 Other_Terminal。printf 必须在 OS 初始化之后调用，
 *         否则互斥锁尚未创建会触发 ThreadX 错误
 */
int _write(int file, char *ptr, int len)
{
    (void)file;
    tx_mutex_get(&AppPrintfSemp, TX_WAIT_FOREVER);
    SEGGER_RTT_SetTerminal(Other_Terminal);
    SEGGER_RTT_Write(0, ptr, len);
    tx_mutex_put(&AppPrintfSemp);
    return len;
}
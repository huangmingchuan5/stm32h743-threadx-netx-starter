/**
 * @file    bsp_RTT_viewer.c
 * @brief   SEGGER RTT viewer 通道初始化 + putchar 重定向
 */
#include "bsp_RTT_viewer.h"
#include "SEGGER_RTT.h"

/**
 * @brief SEGGER RTT viewer的初始化
 */
void RTT_viewer_init(void)
{
    /* 配置通道0，上行配置*/
    SEGGER_RTT_ConfigUpBuffer(0, "RTTUP", NULL, 0,
                              SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    /* 配置通道0，下行配置*/
    SEGGER_RTT_ConfigDownBuffer(0, "RTTDOWN", NULL, 0,
                                SEGGER_RTT_MODE_NO_BLOCK_SKIP);

    //static const unsigned char _aTerminalId[16]; in file SEGGER_RTT.c
    //terminal_max = sizeof(unsigned char _aTerminalId[16])
    for (uint8_t i = 0; i < 16; i++) {
        SEGGER_RTT_SetTerminal(i);
        SEGGER_RTT_WriteString(0, RTT_CTRL_RESET);
        SEGGER_RTT_WriteString(0, RTT_CTRL_CLEAR);
    }
}

/**
 * @brief 重定向putchar函数
 * @param ch putchar的char字节
 * @return
 */
__attribute__((weak)) int __io_putchar(int ch)
{
    SEGGER_RTT_SetTerminal(0);
    SEGGER_RTT_Write(0, &ch, 1);
    return ch;
}
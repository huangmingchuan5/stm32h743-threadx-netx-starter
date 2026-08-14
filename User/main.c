/**
 * @file    main.c
 * @brief   程序入口：System_Init 后挂起 HAL tick，进入 ThreadX 内核
 */
#include "tx_api.h"
#include "bsp.h"
#include "stm32h7xx_hal.h"

/**
 *@brief 主函数
 */
int main(void) {
    System_Init();

    HAL_SuspendTick();

    tx_kernel_enter();

    while (1);
}

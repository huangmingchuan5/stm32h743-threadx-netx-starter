/**
 * @file nx_stm32_eth_config.h
 * @brief STM32H7 NetX Duo Ethernet driver configuration
 */
#ifndef NX_STM32_ETH_CONFIG_H
#define NX_STM32_ETH_CONFIG_H

#include "bsp_eth.h"
#include "lan8742.h"
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ETH_HandleTypeDef heth;

#define eth_handle heth

#define NX_DRIVER_ETH_HW_IP_INIT
#define nx_eth_init bsp_eth

#ifdef __cplusplus
}
#endif

#endif /* NX_STM32_ETH_CONFIG_H */

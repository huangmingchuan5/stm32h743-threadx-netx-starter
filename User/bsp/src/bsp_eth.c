/**
 * @file bsp_eth.c
 * @brief Ethernet board support implementation
 */
#include "bsp_eth.h"
#include <string.h>
#include "stm32h7xx_hal.h"
#include "tx_api.h"
#include "app_eth_config.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */

#pragma location=0x30000000
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT];
/* Ethernet Rx DMA Descriptors */
#pragma location=0x30000080
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT];
/* Ethernet Tx DMA Descriptors */

#elif defined ( __CC_ARM )  /* MDK ARM Compiler */

__attribute__((at(0x30000000))) ETH_DMADescTypeDef DMARxDscrTab[
    ETH_RX_DESC_CNT]; /* Ethernet Rx DMA Descriptors */
__attribute__((at(0x30000080))) ETH_DMADescTypeDef DMATxDscrTab[
    ETH_TX_DESC_CNT]; /* Ethernet Tx DMA Descriptors */

#elif defined ( __GNUC__ ) /* GNU Compiler */

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT] __attribute__((section(
    ".RxDescripSection"))); /* Ethernet Rx DMA Descriptors */
ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT] __attribute__((section(
    ".TxDescripSection"))); /* Ethernet Tx DMA Descriptors */

#endif
ETH_BufferTypeDef Txbuffer[ETH_TX_DESC_CNT * 2U];
ETH_TxPacketConfig TxConfig;

ETH_HandleTypeDef heth;

/**
 * @brief Initialize the Ethernet hardware
 */
void bsp_eth(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN ETH_MspInit 0 */

    /* USER CODE END ETH_MspInit 0 */
    /* ETH clock enable */
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ETH GPIO Configuration
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PB11     ------> ETH_TX_EN
    PB12     ------> ETH_TXD0
    PB13     ------> ETH_TXD1
    */
    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    static uint8_t MACAddr[6];

    /*
     * MAC 从 app_eth_config 取，single source of truth。
     * 调用方必须保证 app_eth_config_init 已经执行（thread_eth_entry 里做的）。
     */
    {
        struct app_eth_config cfg;
        app_eth_config_get_current(&cfg);
        memcpy(MACAddr, cfg.mac, sizeof(MACAddr));
    }

    heth.Instance = ETH;
    heth.Init.MACAddr = &MACAddr[0];
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc = DMATxDscrTab;
    heth.Init.RxDesc = DMARxDscrTab;
    heth.Init.RxBuffLen = 1536;

    HAL_ETH_Init(&heth);

    HAL_ETH_SetMDIOClockRange(&heth);

    memset(&TxConfig, 0, sizeof(ETH_TxPacketConfig));
    TxConfig.Attributes =
        ETH_TX_PACKETS_FEATURES_CSUM | ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.ChecksumCtrl = ETH_CHECKSUM_IPHDR_PAYLOAD_INSERT_PHDR_CALC;
    TxConfig.CRCPadCtrl = ETH_CRC_PAD_INSERT;

    HAL_NVIC_SetPriority(ETH_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(ETH_IRQn);
}

/**
 * @brief Handle the Ethernet global interrupt
 */
void ETH_IRQHandler(void)
{
    HAL_ETH_IRQHandler(&heth);
}

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
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /*
     * 野火 H743X/H750X 底板上 LAN8720A 的 nRST（pin 15）直接接 MCU 的 NRST，
     * 软件无法单独控制 PHY 复位，上电时两者已经一起复位。这里只等 PHY 内部
     * 复位完成、SMI 可访问；LAN8720A 复位释放后需要约 25 ms。
     */
    tx_thread_sleep(50U);

    /**ETH GPIO Configuration
     * 野火_H743X_H750X_底板_V1.0_原理图 第 9 页，U16 = LAN8720A
    PC1     ------> ETH_MDC           (PHY pin 13)
    PA1     ------> ETH_REF_CLK       (PHY pin 14, NINT/REFCLKO 输出 50 MHz)
    PA2     ------> ETH_MDIO          (PHY pin 12)
    PA7     ------> ETH_CRS_DV        (PHY pin 11, CSR_DV/MODE2)
    PC4     ------> ETH_RXD0          (PHY pin 8,  RXD0/MODE0)
    PC5     ------> ETH_RXD1          (PHY pin 7,  RXD1/MODE1)
    PB11    ------> ETH_TX_EN         (PHY pin 16)
    PG13    ------> ETH_TXD0          (PHY pin 17)
    PG12    ------> ETH_TXD1          (PHY pin 18)
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

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

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

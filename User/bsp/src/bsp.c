/**
 * @file    bsp.c
 * @brief   板级支持：System_Init（HAL + 时钟 + RTT），bsp_Init（外设、OS 内调用）
 */
#include "bsp.h"
#include "bsp_RTT_viewer.h"
#include "stm32h7xx_hal.h"

void SystemClock_Config(void);

void MPU_Config(void);

/**
 * @brief 系统初始化，时钟初始化和必要组件的初始化。
 * @note 请在运行最初调用。
 */
void System_Init(void)
{
    /* MPU Configuration */
    MPU_Config();

    /* Enable I-Cache */
    SCB_EnableICache();

    /* Enable D-Cache */
    SCB_EnableDCache();

    /* HAL初始化 */
    HAL_Init();

    /* 时钟初始化 */
    SystemClock_Config();

    /* RTT_Viewer初始化 */
    RTT_viewer_init();
}

/**
 * @brief 外设、硬件初始化，会在操作系统内调用。
 */
void bsp_Init(void)
{
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Supply configuration update enable
    */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

    /** Configure the main internal regulator output voltage
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
    }

    /** Initializes the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 2;
    RCC_OscInitStruct.PLL.PLLN = 44;
    RCC_OscInitStruct.PLL.PLLP = 1;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    /** Initializes the CPU, AHB and APB buses clocks
    */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                                  | RCC_CLOCKTYPE_D3PCLK1 |
                                  RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3);
}

/**
 * @brief MPU Configuration
 */
void MPU_Config(void)
{
    MPU_Region_InitTypeDef mpu_init = {0};

    HAL_MPU_Disable();

    mpu_init.Enable = MPU_REGION_ENABLE;
    mpu_init.Number = MPU_REGION_NUMBER0;
    mpu_init.BaseAddress = 0x30000000;
    mpu_init.Size = MPU_REGION_SIZE_32KB;
    mpu_init.SubRegionDisable = 0x00;
    mpu_init.TypeExtField = MPU_TEX_LEVEL0;
    mpu_init.AccessPermission = MPU_REGION_FULL_ACCESS;
    mpu_init.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    mpu_init.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    mpu_init.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    mpu_init.IsBufferable = MPU_ACCESS_BUFFERABLE;

    HAL_MPU_ConfigRegion(&mpu_init);

    mpu_init.Number = MPU_REGION_NUMBER1;
    mpu_init.BaseAddress = 0x24000000;
    mpu_init.Size = MPU_REGION_SIZE_64KB;
    mpu_init.SubRegionDisable = 0x00;
    mpu_init.TypeExtField = MPU_TEX_LEVEL1;
    mpu_init.AccessPermission = MPU_REGION_FULL_ACCESS;
    mpu_init.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    mpu_init.IsShareable = MPU_ACCESS_SHAREABLE;
    mpu_init.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    mpu_init.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&mpu_init);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

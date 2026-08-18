/**
 * @file boot_main.c
 * @brief Bootloader entry and application handoff.
 */
#include "stm32h7xx.h"
#include "bsp_RTT_viewer.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(APP_FLASH_START_ADDRESS) || !defined(APP_FLASH_LENGTH)
#error "Application FLASH layout is not configured"
#endif

#define FLASH_END_ADDRESS (APP_FLASH_START_ADDRESS + APP_FLASH_LENGTH)
#define DTCM_START_ADDRESS (0x20000000UL)
#define DTCM_END_ADDRESS (0x20020000UL)

/**
 * @brief Check whether the application vector table contains valid entry values.
 * @param app_stack Initial application main stack pointer.
 * @param app_reset Application reset handler address.
 * @return true when the vector table can be used for handoff.
 */
static bool boot_app_is_valid(uint32_t app_stack, uint32_t app_reset)
{
    const uint32_t reset_address = app_reset & ~1UL;
    const bool stack_is_valid = app_stack >= DTCM_START_ADDRESS
                                && app_stack <= DTCM_END_ADDRESS;
    const bool reset_is_valid = (app_reset & 1UL) != 0UL
                                && reset_address >= APP_FLASH_START_ADDRESS
                                && reset_address < FLASH_END_ADDRESS;

    return stack_is_valid && reset_is_valid;
}

/**
 * @brief Stop core interrupts and branch to the application reset handler.
 * @param app_stack Initial application main stack pointer.
 * @param app_reset Application reset handler address.
 */
__attribute__((noreturn)) static void boot_jump_to_app(uint32_t app_stack, uint32_t app_reset)
{
    __disable_irq();

    SysTick->CTRL = 0UL;
    SysTick->LOAD = 0UL;
    SysTick->VAL = 0UL;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;

    for (uint32_t index = 0; index < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); ++index) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }

    SCB->VTOR = APP_FLASH_START_ADDRESS;
    __set_CONTROL(0UL);
    __DSB();
    __ISB();

    __asm volatile(
        "msr msp, %0\n"
        "bx %1\n"
        :
        : "r"(app_stack), "r"(app_reset)
        : "memory"
    );

    __builtin_unreachable();
}

/**
 * @brief Validate and start the application image.
 * @return This function does not return after a successful handoff.
 */
int main(void)
{
    RTT_viewer_init();
    printf("\r\n[BOOT] Bootloader started.\r\n");
    printf("[BOOT] Application region: 0x%08lX - 0x%08lX.\r\n",
           (unsigned long)APP_FLASH_START_ADDRESS,
           (unsigned long)FLASH_END_ADDRESS);

    const uint32_t app_stack = *(const volatile uint32_t *)APP_FLASH_START_ADDRESS;
    const uint32_t app_reset = *(const volatile uint32_t *)(APP_FLASH_START_ADDRESS + sizeof(uint32_t));

    printf("[BOOT] Application vector: MSP=0x%08lX, Reset=0x%08lX.\r\n",
           (unsigned long)app_stack,
           (unsigned long)app_reset);

    if (boot_app_is_valid(app_stack, app_reset)) {
        printf("[BOOT] Application is valid, jumping to application.\r\n");
        boot_jump_to_app(app_stack, app_reset);
    }

    printf("[BOOT] Application is invalid, bootloader halted.\r\n");

    while (1) {
    }
}

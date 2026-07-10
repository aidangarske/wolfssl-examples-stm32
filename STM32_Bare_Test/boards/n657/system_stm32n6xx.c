/* system_stm32n6xx.c - minimal CMSIS SystemInit for STM32N657
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Stripped-down replacement for ST's full system_stm32n6xx.c which
 * depends on HAL_PWR/HAL_RCC/HAL_NVIC. All real clock / VTOR / FPU
 * setup happens in board_init() (hw_init.c) after this returns;
 * here we just satisfy the CMSIS contract that the startup file calls.
 */

#include "stm32n6xx.h"
#include <stdint.h>

#if !defined(VECT_TAB_OFFSET)
    #define VECT_TAB_OFFSET 0x0u
#endif

uint32_t SystemCoreClock = 64000000u; /* HSI 64 MHz default */

void SystemInit(void)
{
    /* FPU enable (CP10/CP11 full access). Done again in board_init() for
     * symmetry with other boards. */
    SCB->CPACR |= ((3u << 20) | (3u << 22));
    __DSB();
    __ISB();

    /* Vector table at image base (linker places .isr_vector at ORIGIN(ROM)
     * which is 0x34000000 for the LRUN linker script). */
    SCB->VTOR = (uint32_t)0x34000000u + VECT_TAB_OFFSET;
}

void SystemCoreClockUpdate(void)
{
    /* Derive the actual CPUCLK from the RCC CPU clock-switch status.
     * CPUSWS (RCC->CFGR1[21:20]): 0 = HSI 64 MHz (boot default), 3 = IC1.
     * clock_init() brings IC1 up as PLL1/2 = 600 MHz; before it runs the
     * Boot ROM leaves the CPU on HSI. (Other CPUSWS sources -- MSI/HSE --
     * are not used by this board, so they map to the HSI default.) */
    if ((RCC->CFGR1 & RCC_CFGR1_CPUSWS_Msk) ==
        (0x3u << RCC_CFGR1_CPUSWS_Pos)) {
        SystemCoreClock = 600000000u;
    }
    else {
        SystemCoreClock = 64000000u;
    }
}

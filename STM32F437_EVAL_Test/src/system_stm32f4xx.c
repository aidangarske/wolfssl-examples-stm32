/* system_stm32f4xx.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * Minimal CMSIS system file for STM32F4xx.
 *
 * Provides the three CMSIS-mandated symbols:
 *   - SystemInit()           called by Reset_Handler before main()
 *   - SystemCoreClock        global SYSCLK in Hz, used by SysTick / drivers
 *   - SystemCoreClockUpdate() recomputes SystemCoreClock from RCC registers
 *
 * Application clock setup lives in hw_init.c; this file therefore keeps
 * SystemInit() to the bare minimum (FPU enable + optional VTOR offset).
 */

#include "stm32f4xx.h"

/* HSI_VALUE / HSE_VALUE come from the CMSIS device header; provide safe
 * fallbacks in case a board-specific override removed them. */
#ifndef HSI_VALUE
#define HSI_VALUE   16000000U
#endif
#ifndef HSE_VALUE
#define HSE_VALUE   25000000U
#endif

/* Default vector-table offset within flash. Override by defining
 * USER_VECT_TAB_ADDRESS and VECT_TAB_OFFSET on the command line. */
#ifndef VECT_TAB_OFFSET
#define VECT_TAB_OFFSET 0x00000000U
#endif

/* Public globals (CMSIS-mandated names). */
uint32_t SystemCoreClock = HSI_VALUE;

/* AHB / APB prescaler decode tables. The CMSIS device header declares
 * these as externs; defining them here satisfies any HAL or driver code
 * that imports them. Values are right-shift counts. */
const uint8_t AHBPrescTable[16] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9
};
const uint8_t APBPrescTable[8]  = { 0, 0, 0, 0, 1, 2, 3, 4 };

void SystemInit(void)
{
#if defined(__FPU_PRESENT) && (__FPU_PRESENT == 1U) && \
    defined(__FPU_USED)    && (__FPU_USED    == 1U)
    /* Grant full access to coprocessors CP10 and CP11 (FPU). */
    SCB->CPACR |= ((3U << (10U * 2U)) | (3U << (11U * 2U)));
#endif

#ifdef USER_VECT_TAB_ADDRESS
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET;
#endif
}

void SystemCoreClockUpdate(void)
{
    uint32_t cfgr;
    uint32_t pllcfgr;
    uint32_t sysclk;
    uint32_t pllsrc;
    uint32_t pllm;
    uint32_t plln;
    uint32_t pllp;
    uint32_t pllinput;
    uint32_t hpre;

    cfgr = RCC->CFGR;

    /* SWS bits [3:2] tell us which source is currently driving SYSCLK. */
    switch (cfgr & RCC_CFGR_SWS) {
        case RCC_CFGR_SWS_HSI:
            sysclk = HSI_VALUE;
            break;
        case RCC_CFGR_SWS_HSE:
            sysclk = HSE_VALUE;
            break;
        case RCC_CFGR_SWS_PLL:
            pllcfgr = RCC->PLLCFGR;
            pllsrc  = pllcfgr & RCC_PLLCFGR_PLLSRC;
            pllm    = (pllcfgr & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos;
            plln    = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
            /* PLLP is encoded as (P/2 - 1) in bits, so decode to {2,4,6,8}. */
            pllp    = (((pllcfgr & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos)
                       + 1U) * 2U;

            if (pllm == 0U) {
                /* Invalid divider — fall back to HSI to avoid div-by-zero. */
                sysclk = HSI_VALUE;
            }
            else {
                pllinput = (pllsrc == RCC_PLLCFGR_PLLSRC_HSE)
                           ? HSE_VALUE : HSI_VALUE;
                sysclk = ((pllinput / pllm) * plln) / pllp;
            }
            break;
        default:
            sysclk = HSI_VALUE;
            break;
    }

    /* HPRE bits [7:4] of CFGR drive the AHB prescaler. */
    hpre = AHBPrescTable[(cfgr & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
    SystemCoreClock = sysclk >> hpre;
}

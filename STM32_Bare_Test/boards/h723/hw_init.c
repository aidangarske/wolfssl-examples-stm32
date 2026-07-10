/* hw_init.c - STM32H723ZG (NUCLEO-H723ZG), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-H723ZG:
 *   - HSI 64 MHz at reset; keep it as SYSCLK (no PLL bring-up for first
 *     light). H723 can run to 550 MHz with PLL -- can be added later.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK V3 VCP on the
 *     NUCLEO-H723ZG (same Nucleo-144 layout as H753).
 *   - Cortex-M7.
 *
 * H723xx silicon HW crypto: RNG only. H72x sub-family does not have
 * CRYP/HASH peripherals; only H72x/H73x with the "high-security"
 * suffix include them (currently shipping H7B3/H7A3).
 */

#include "stm32h7xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART3->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void)
{
    /* Stay at HSI 64 MHz. H7 default after reset is HSION=1, HSIDIV=/1
     * (== 64 MHz), CFGR.SW=HSI. No flash latency change required for
     * 64 MHz at VOS Scale 3 (default, 0 WS). */

    /* Enable HSI48 (RNG kernel clock source on H7). */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }

    /* USART2/3/4/5/7/8 kernel clock = PCLK1 (default). Some chips
     * boot with USART234578SEL non-zero -- force PCLK1. */
    RCC->D2CCIP2R &= ~RCC_D2CCIP2R_USART28SEL_Msk;
}

static void uart_init(void)
{
    /* GPIOD clock (AHB4 on H7) + USART3 clock (APB1 low). */
    RCC->AHB4ENR  |= RCC_AHB4ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    /* PD8 (TX) / PD9 (RX) AF7, USART3 at PCLK1 = 64 MHz (HSI default). */
    board_common_uart_pin_init(GPIOD, 8u, 9u, 7u);
    board_common_uart_basic_init(USART3, 64000000u, 115200u);
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M7F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(64000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 64000000u;
}


const char *board_name(void)
{
    return "NUCLEO-H723ZG";
}

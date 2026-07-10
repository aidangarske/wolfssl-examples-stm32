/* hw_init.c - STM32H7S3L8 (NUCLEO-H7S3L8), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-H7S3L8:
 *   - HSI 64 MHz at reset; keep it as SYSCLK (no PLL bring-up for first
 *     light). H7S can run to 600 MHz with PLL -- can be added later.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK V3 VCP on the
 *     NUCLEO-H7S3L8 (same Nucleo-144 layout as H723/H753).
 *   - Cortex-M7.
 *
 * H7S3L8 silicon HW crypto: CRYP (classic fat IP, same shape as H753) +
 * HASH (classic, same shape as H753) + RNG + SAES (DHUK-capable) + V2
 * PKA (same shape as U5/H5). All five peripherals clocked off AHB3ENR
 * (vs classic H7 which has CRYP/HASH/RNG on AHB2). This is the first
 * STM32H7-family port to expose a V2 PKA + SAES.
 *
 * Boot path: H7S has only 64 KB of internal user flash (FSBL/CCB use
 * only). The application loads to AXI SRAM at 0x24000000 via the ST-LINK
 * debugger (see stm32h7s3_lrun.ld for the layout) -- no internal flash
 * programming or XSPI configuration required for this BARE bench.
 */

#include "stm32h7rsxx.h"
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
    /* Stay at HSI 64 MHz. H7S boot defaults: HSION=1, HSIDIV=/1
     * (== 64 MHz), CFGR.SW=HSI. No flash latency change required for
     * 64 MHz at VOS Scale 3 (default, 0 WS). */

    /* APB1 prescaler defaults to /1 after reset, so PCLK1 = HCLK = 64 MHz.
     * USART3 kernel clock defaults to PCLK1 (CCIPR2 USART234578SEL = 0),
     * which is what we want. No explicit programming needed. */

    /* Enable HSI48 -- the RNG kernel clock on H7S is hard-wired to
     * HSI48 (no CCIPR muxing). Without this, RNG_SR.DRDY never asserts
     * and wc_GenerateSeed returns RNG_FAILURE_E (-199). */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }
}

static void uart_init(void)
{
    /* GPIOD clock (AHB4 on H7RS) + USART3 clock (APB1 ENR1). */
    RCC->AHB4ENR  |= RCC_AHB4ENR_GPIODEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN;
    (void)RCC->APB1ENR1;

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
    return "NUCLEO-H7S3L8";
}

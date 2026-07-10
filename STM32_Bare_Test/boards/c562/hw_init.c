/* hw_init.c - STM32C562RE (NUCLEO-C562RE), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-C562RE. Adapted from the
 * boards/c5a3/ port (same C5 family RCC + GPIO + USART register
 * shape, same Cortex-M33 core). Differences vs C5A3:
 *
 *   - Smaller die: 512 KB flash + 128 KB SRAM (vs 1 MB + 256 KB)
 *   - NUCLEO-64 form factor (vs NUCLEO-144 on C5A3ZG); ST-LINK VCP
 *     wiring is the standard NUCLEO-64 USART2 PA2/PA3 AF7, same as
 *     the C5A3 port (the C5A3ZG NUCLEO-144 also routes to USART2
 *     rather than the typical NUCLEO-144 USART3 PD8/PD9)
 *   - No HSE: NUCLEO-C562RE has no soldered HSE crystal. SYSCLK is
 *     sourced directly from HSIS (HSI-for-System, 144 MHz native on
 *     the C5 family) -- no PSI bring-up needed for first-light
 *
 * Crypto inventory matches C5A3: CRYP-shape AES + HASH + RNG + SAES
 * + V2 PKA on AHB2. wolfssl-side WOLFSSL_STM32C5 arm covers C562
 * without changes.
 */

#include "stm32c562xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* CMSIS-Core hooks. Vendor startup_stm32c562xx.c calls SystemInit() and
 * __PROGRAM_START() during reset. SystemInit() is a no-op (we set up
 * clocks ourselves); __PROGRAM_START runs the C-runtime init then
 * jumps to main(). */
void SystemInit(void) { /* no-op */ }
__attribute__((weak)) void Default_IRQHandler_Hook(void) {
    while (1) { }
}

#define SW_TIMEOUT      0x010000u

#define BOARD_SYSCLK_HZ 144000000u

/* ---- printf retarget over USART2 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init: HSIS = 144 MHz SYSCLK -------------------------------
 * NUCLEO-C562RE does not have a soldered HSE crystal (NUCLEO-64 form
 * factor), so we drive SYSCLK from HSIS (the C5 HSI-for-System variant,
 * 144 MHz native). The RNG kernel clock is CK48, sourced from HSIDIV3 =
 * HSI/3 = 48 MHz (see below). */
static void clock_init(void)
{
    uint32_t reg;
    volatile uint32_t timeout;

    /* 1. RNG kernel clock = CK48 (RCC_CCIPR2.CK48SEL); its reset default is
     * NONE, so without this the RNG has no kernel clock and conditioning
     * stalls (SR.BUSY stuck, CONDRST never clears). Enable HSIDIV3 (HSI/3 =
     * 48 MHz, internal -- the NUCLEO-C562RE has no HSE crystal) and select it
     * as the CK48 source. Same fix as boards/c5a3/hw_init.c (validated on the
     * C5A3; C562 shares the C5 RNG IP but is not on the bench to re-verify). */
    RCC->CR1 |= RCC_CR1_HSIDIV3ON;
    timeout = SW_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    reg = RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk;
    reg |= RCC_CCIPR2_CK48SEL_1;   /* CK48 source = HSIDIV3 (HSI/3 = 48 MHz) */
    RCC->CCIPR2 = reg;

    /* 2. HSIS on (System HSI = 144 MHz). At reset the C5 may boot on
     * a lower or gated clock; assert HSISON explicitly so the system
     * bus reaches the assumed 144 MHz before we set BRR. */
    RCC->CR1 |= RCC_CR1_HSISON;
    timeout = SW_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSISRDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    if (timeout == 0u) {
        return;
    }

    /* 3. All bus prescalers /1. Reset value is already 0; explicit for
     * intent so PCLK1 = HCLK = SYSCLK = 144 MHz. */
    RCC->CFGR2 = 0u;

    /* 4. Flash 4 WS + prefetch BEFORE relying on 144 MHz HCLK. */
    reg = FLASH->ACR & ~FLASH_ACR_LATENCY;
    reg |= (4u << FLASH_ACR_LATENCY_Pos) | FLASH_ACR_PRFTEN;
    FLASH->ACR = reg;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) !=
           (4u << FLASH_ACR_LATENCY_Pos)) {
        /* spin */
    }

    /* 5. Select HSIS as SYSCLK (SW = 01). Reset value is SW = 00 which
     * selects HSIDIV3 = HSI/3 = 48 MHz, NOT HSIS. The vendor
     * system_stm32c5xx.c documents the SW encoding:
     *   00 -> HSIDIV3 (48 MHz)
     *   01 -> HSIS    (144 MHz)
     *   10 -> HSE     (external)
     *   11 -> PSIS    (PLL output, used by C5A3) */
    reg = RCC->CFGR1 & ~RCC_CFGR1_SW;
    RCC->CFGR1 = reg | RCC_CFGR1_SW_0;
    timeout = SW_TIMEOUT;
    while (((RCC->CFGR1 & RCC_CFGR1_SWS) != RCC_CFGR1_SWS_0) &&
           (--timeout != 0u)) {
        /* spin */
    }

    /* 6. Programming delay 2 (required at HCLK >= 136 MHz). */
    reg = FLASH->ACR & ~FLASH_ACR_WRHIGHFREQ;
    FLASH->ACR = reg | (2u << FLASH_ACR_WRHIGHFREQ_Pos);
}

/* ---- USART2 init: PA2 (TX) / PA3 (RX), AF7 ---------------------------- */
static void uart_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) |
                    (2u << GPIO_MODER_MODE3_Pos);
    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (7u << GPIO_AFRL_AFSEL2_Pos) |
                     (7u << GPIO_AFRL_AFSEL3_Pos);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2_Msk | GPIO_PUPDR_PUPD3_Msk);

    RCC->APB1LENR |= RCC_APB1LENR_USART2EN;
    (void)RCC->APB1LENR;

    USART2->CR1 = 0;
    USART2->BRR = BOARD_SYSCLK_HZ / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M33F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(BOARD_SYSCLK_HZ);

    /* Vendor SystemInit is overridden with a no-op above, so nothing
     * sets SystemCoreClock for the CMSIS-reported banner readout.
     * Update it explicitly to the actual post-clock_init() SYSCLK. */
    SystemCoreClock = BOARD_SYSCLK_HZ;
}

uint32_t board_sysclk_hz(void)
{
    return BOARD_SYSCLK_HZ;
}


const char *board_name(void)
{
    return "NUCLEO-C562RE";
}

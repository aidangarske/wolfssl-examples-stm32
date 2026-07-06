/* hw_init.c - NUCLEO-U083RC (STM32U083RC), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-U083RC:
 *   - HSI 16 MHz at reset; keep as SYSCLK (no PLL bring-up). U0 family
 *     supports up to 56 MHz on the PLL but the lab board runs the
 *     wolfcrypt test fine at 16 MHz; PLL bring-up is a follow-up if
 *     we need the throughput.
 *   - USART2 on PA2 (TX) / PA3 (RX) AF1, 115200 8N1, ST-LINK V2-1 VCP.
 *   - HSI48 enabled for RNG kernel clock.
 *
 * U083 has TinyAES + RNG only -- no SAES, no HASH, no PKA, no CRYP.
 */

#include "stm32u083xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART2 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* After reset on U0, the default SYSCLK source is MSI at 4 MHz (NOT HSI
 * like on U5). Switch SYSCLK to HSI 16 MHz so PCLK is 16 MHz -- this
 * also keeps the BRR math simple for the UART. Latency stays at 0 WS
 * (U0 supports up to 56 MHz at the default voltage range). */
static void clock_init(void)
{
    /* Enable HSI 16 MHz */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* Switch SYSCLK to HSI (CFGR.SW = 001 -> HSI16) */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_0;
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 1u) { }

    /* HSI48 for the RNG kernel clock. Unlike U5/H5, the default CCIPR
     * CLK48SEL on U0 is "NONE" -- without routing a clock to the RNG
     * the IP immediately reports SECS=1 on the first RNGEN. Select
     * HSI48 explicitly. */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_CLK48SEL_Msk) |
                 RCC_CCIPR_CLK48SEL;  /* CLK48SEL = 11 -> HSI48 */
}

/* ---- USART2 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOA on IOPENR (U0 uses IOPENR, not AHB2ENR like U5). */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    (void)RCC->IOPENR;

    /* PA2 / PA3 MODER = AF (10b), AF7 (USART2) per U073/U083 alternate
     * function table. AF1 on PA2 routes to TIM2_CH3, not USART2. */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);

    /* AFR[0] holds AF for pins 0..7 */
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (7u << GPIO_AFRL_AFSEL2_Pos) |
                     (7u << GPIO_AFRL_AFSEL3_Pos);

    /* Enable USART2 on APBENR1 */
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    (void)RCC->APBENR1;

    /* USART2: 8N1, oversample 16. APB1 = HSI 16 MHz; BRR = clk / baud. */
    USART2->CR1 = 0;
    USART2->BRR = 16000000u / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(16000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 16000000u;
}


const char *board_name(void)
{
    return "NUCLEO-U083RC";
}

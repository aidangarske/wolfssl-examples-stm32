/* hw_init.c - STM32C031C6 (NUCLEO-C031C6), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-C031C6:
 *   - HSI 48 MHz with HSIDIV=/4 -> 12 MHz SYSCLK at reset (the C0
 *     default after SystemInit; SystemCoreClock initializer is
 *     12000000UL). Keep that for first light; no PLL on the C0.
 *   - USART2 on PA2 (TX) / PA3 (RX) AF1, 115200 8N1, ST-LINK V2-1 VCP.
 *   - Cortex-M0+. No FPU.
 *
 * C031C6 silicon HW crypto: NONE. The C0 family is the cost-reduced
 * counterpart to G0 -- no AES, no HASH, no RNG, no PKA. Everything in
 * software including DRBG. 32 KB flash / 12 KB RAM is the smallest in
 * the matrix; even the G071 trim does not fit, so STM32_C031_TRIM
 * disables wolfcrypt_test entirely and runs only the main_test.c KAT
 * subset (SHA-256 + AES-CBC + AES-ECB + DRBG-seeded RNG smoke).
 */

#include "stm32c0xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART2 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void)
{
    /* SystemInit leaves SYSCLK at HSI48 / HSIDIV=4 = 12 MHz. C0 has no
     * PLL. Latency 0 WS at 12 MHz; no FLASH->ACR change needed. */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
}

static void uart_init(void)
{
    /* Enable GPIOA clock (IOPENR bit GPIOAEN) */
    RCC->IOPENR |= RCC_IOPENR_GPIOAEN;
    (void)RCC->IOPENR;

    /* PA2 (TX), PA3 (RX): MODER = AF (10b), AF1 (USART2) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);

    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (1u << GPIO_AFRL_AFSEL2_Pos) |
                     (1u << GPIO_AFRL_AFSEL3_Pos);

    /* Enable USART2 clock (APBENR1 bit USART2EN). C0 unifies APB1/APB2
     * into a single APBENR1 register, same as G0. */
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    (void)RCC->APBENR1;

    /* USART2: 8N1, oversampling 16. PCLK = HCLK = SYSCLK = 12 MHz. */
    USART2->CR1 = 0;
    USART2->BRR = 12000000u / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(12000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 12000000u;
}


const char *board_name(void)
{
    return "NUCLEO-C031C6";
}

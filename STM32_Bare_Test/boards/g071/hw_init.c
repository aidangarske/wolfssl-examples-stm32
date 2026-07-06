/* hw_init.c - STM32G071RB (NUCLEO-G071RB), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-G071RB:
 *   - HSI16 (16 MHz) as SYSCLK -- no PLL bring-up for first light.
 *     G0 can run to 64 MHz with PLL; can be added later if benchmarks
 *     are wanted.
 *   - USART2 on PA2 (TX) / PA3 (RX) AF1, 115200 8N1, ST-LINK V2-1 VCP.
 *   - Cortex-M0+. No FPU.
 *
 * G071 silicon HW crypto: NONE. The G071 sub-family has no AES, no
 * HASH, no RNG, no PKA peripheral (see stm32g071xx.h: only RNG_TypeDef-
 * adjacent symbols are notably absent). All cryptography falls back to
 * software, including DRBG -- HASH_DRBG seeded by HW timer entropy.
 * This board exercises the pure-SW path on a Cortex-M0+ (smaller test
 * footprint via STM32_G071_TRIM, modeled on STM32_U083_TRIM).
 */

#include "stm32g0xx.h"
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
    /* Stay at HSI16 = 16 MHz. After reset: HSION=1, HSIRDY=1, SW=HSI16.
     * No flash latency change needed at 16 MHz (0 WS). */
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

    /* AFRL: PA2 -> AFR[0] bits[11:8]; PA3 -> AFR[0] bits[15:12]; AF1 = 0x1 */
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (1u << GPIO_AFRL_AFSEL2_Pos) |
                     (1u << GPIO_AFRL_AFSEL3_Pos);

    /* Enable USART2 clock (APBENR1 bit USART2EN) */
    RCC->APBENR1 |= RCC_APBENR1_USART2EN;
    (void)RCC->APBENR1;

    /* USART2: 8N1, oversampling 16. PCLK = HCLK = SYSCLK = 16 MHz. */
    USART2->CR1 = 0;
    USART2->BRR = 16000000u / 115200u;
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
    board_common_systick_init(16000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 16000000u;
}


const char *board_name(void)
{
    return "NUCLEO-G071RB";
}

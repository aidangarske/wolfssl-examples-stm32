/* hw_init.c - STM32L562QEI (STM32L562E-DK), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for the STM32L562E-DK Discovery Kit:
 *   - MSI 4 MHz default at reset; switch SYSCLK to HSI 16 MHz so the
 *     ECC SP-math + UART have enough clock without bringing up the
 *     PLL. (PLL bring-up is a later optimization for bench numbers.)
 *   - USART1 on PA9 (TX) / PA10 (RX) AF7 -- ST-LINK V3E VCP path on
 *     the STM32L562E-DK. PA[9:10] are in the standard VddIO domain
 *     so no VddIO2 isolate release is needed (different from L552ZE-Q
 *     which uses LPUART1 on PG[7:8]).
 *   - USART1 kernel clock left on PCLK2 default (= HSI = 16 MHz).
 *
 * L562 has TinyAES + HASH + RNG + PKA (V1) + SAES + DHUK -- full
 * crypto set vs L552 which only has HASH + RNG. TrustZone (TZEN)
 * already disabled on the unit we're flashing.
 */

#include "stm32l5xx.h"
#include <stdio.h>
#include <stdint.h>
#include "board.h"

void board_putc(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART1->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void) {
    /* Switch SYSCLK from MSI 4 MHz default to HSI 16 MHz. */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    /* CFGR.SW: 00=MSI (default), 01=HSI16, 10=HSE, 11=PLL. SW_0=01=HSI. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_0;
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 1u) { }

    /* Enable HSI48 (RNG kernel clock source on L5). */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }
}

static void uart_init(void) {
    /* GPIOA clock. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    /* PA9 (TX), PA10 (RX): MODER = AF (10b), AF7 (USART1). */
    GPIOA->MODER &= ~(GPIO_MODER_MODE9_Msk | GPIO_MODER_MODE10_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE9_Pos) | (2u << GPIO_MODER_MODE10_Pos);
    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED9_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED10_Pos);
    GPIOA->AFR[1] &= ~((0xFu << GPIO_AFRH_AFSEL9_Pos) |
                       (0xFu << GPIO_AFRH_AFSEL10_Pos));
    GPIOA->AFR[1] |= (7u << GPIO_AFRH_AFSEL9_Pos) |
                     (7u << GPIO_AFRH_AFSEL10_Pos);

    /* USART1 clock on APB2ENR. */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* USART1 kernel clock = PCLK2 (default after CCIPR1.USART1SEL=0).
     * With SYSCLK=HSI 16 MHz and APB2 prescaler /1, PCLK2 = 16 MHz.
     * Standard oversample-16 BRR = fck / baud = 16e6 / 115200 = 139. */
    USART1->CR1 = 0;
    USART1->BRR = 16000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}



void board_init(void) {
    /* Enable FPU (CP10/11 full access). */
    SCB->CPACR |= (0xFu << 20);
    __DSB(); __ISB();
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(16000000u);
}
uint32_t board_sysclk_hz(void) { return 16000000u; }
const char *board_name(void) { return "STM32L562E-DK"; }

/* hw_init.c - STM32L552ZE (NUCLEO-L552ZE-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-L552ZE-Q:
 *   - MSI 4 MHz default at reset; switch SYSCLK to HSI 16 MHz so the
 *     ECC SP-math + UART have enough clock without bringing up the
 *     PLL. (PLL bring-up is a later optimization for bench numbers.)
 *   - LPUART1 on PG7 (TX) / PG8 (RX) AF8 -- ST-LINK V3 VCP path on
 *     the NUCLEO-L552ZE-Q. Port G [7:8] live in the VddIO2 power
 *     domain so PWR.CR2.IOSV must be set before driving them.
 *   - LPUART1 kernel clock routed to HSI 16 MHz (RCC_CCIPR1.LPUART1SEL).
 *
 * L552 has HASH + RNG (no AES, no PKA). TrustZone (TZEN) must be
 * disabled at the option-byte level for the BARE build to run from
 * 0x08000000 in non-secure state; the binary itself is plain M33.
 */

#include "stm32l5xx.h"
#include <stdio.h>
#include <stdint.h>
#include "board.h"

void board_putc(int ch)
{
    while ((LPUART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    LPUART1->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void) {
    /* Switch SYSCLK from MSI 4 MHz default to HSI 16 MHz. MSI stays
     * running (default range 6 = 4 MHz). */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    /* CFGR.SW: 00=MSI (default), 01=HSI16, 10=HSE, 11=PLL.
     * SW_0=01=HSI. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_0;
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 1u) { }

    /* Enable HSI48 (RNG kernel clock source on L5). */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }

    /* Route LPUART1 kernel clock to HSI16 (CCIPR1.LPUART1SEL = 10b).
     * The CMSIS bit-field is LPUART1SEL[1:0] at bit 10. */
    RCC->CCIPR1 = (RCC->CCIPR1 & ~RCC_CCIPR1_LPUART1SEL_Msk) |
                  RCC_CCIPR1_LPUART1SEL_1;
}

static void uart_init(void) {
    /* GPIOG is in the VddIO2 power domain -- isolate must be removed
     * before driving its pins. Enable PWR clock then set CR2.IOSV. */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;
    PWR->CR2 |= PWR_CR2_IOSV;
    (void)PWR->CR2;

    /* GPIOG clock. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOGEN;
    (void)RCC->AHB2ENR;

    /* PG7 (TX), PG8 (RX): MODER = AF (10b), AF8 (LPUART1). */
    GPIOG->MODER &= ~(GPIO_MODER_MODE7_Msk | GPIO_MODER_MODE8_Msk);
    GPIOG->MODER |= (2u << GPIO_MODER_MODE7_Pos) | (2u << GPIO_MODER_MODE8_Pos);
    GPIOG->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED7_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED8_Pos);
    GPIOG->AFR[0] &= ~(0xFu << GPIO_AFRL_AFSEL7_Pos);
    GPIOG->AFR[0] |= (8u << GPIO_AFRL_AFSEL7_Pos);
    GPIOG->AFR[1] &= ~(0xFu << GPIO_AFRH_AFSEL8_Pos);
    GPIOG->AFR[1] |= (8u << GPIO_AFRH_AFSEL8_Pos);

    /* LPUART1 clock on APB1ENR2.LPUART1EN. */
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    (void)RCC->APB1ENR2;

    /* LPUART BRR = (256 * fck) / baud. At HSI16 = 16 MHz, 115200 -> 35556. */
    LPUART1->CR1 = 0;
    LPUART1->BRR = (256u * 16000000u) / 115200u;
    LPUART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    while ((LPUART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
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
const char *board_name(void) { return "NUCLEO-L552ZE-Q"; }

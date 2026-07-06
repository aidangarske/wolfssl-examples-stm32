/* hw_init.c - STM32G474RE (NUCLEO-G474RE), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-G474RE. Pin/clock layout is
 * identical to NUCLEO-G491RE (same G4 family, same Cortex-M4F core):
 *   - HSI16 -> PLL: M=4, N=85, R=2 -> 340 MHz VCO / 2 = 170 MHz SYSCLK
 *   - VOS Range 1 Boost (R1MODE=0), FLASH 4 WS at 170 MHz
 *   - LPUART1 on PA2 (TX) / PA3 (RX) AF12 routed to ST-LINK V3 VCP
 *   - HSI48 enabled as RNG kernel clock source
 *
 * G474 silicon: RNG + PKA (V1) on AHB2. NO AES, NO HASH -- G474 is a
 * non-crypto G4 variant (AES is only on G414 / G441 / G483 / G484 /
 * G4A1 in the G4 family).
 */

#include "stm32g4xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

void board_putc(int ch)
{
    while ((LPUART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    LPUART1->TDR = (uint32_t)ch & 0xFFu;
}

static void clock_init(void)
{
    uint32_t reg;

    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | PWR_CR1_VOS_0;
    while ((PWR->SR2 & PWR_SR2_VOSF) != 0u) { }
    PWR->CR5 &= ~PWR_CR5_R1MODE;

    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk)
               | (4u << FLASH_ACR_LATENCY_Pos)
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while (((FLASH->ACR & FLASH_ACR_LATENCY_Msk) >> FLASH_ACR_LATENCY_Pos) != 4u) { }

    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    reg = 0u;
    reg |= (0x2u << RCC_PLLCFGR_PLLSRC_Pos);   /* HSI16 */
    reg |= (3u   << RCC_PLLCFGR_PLLM_Pos);     /* M = 4 (raw 3) */
    reg |= (85u  << RCC_PLLCFGR_PLLN_Pos);     /* N = 85 */
    reg |= (0u   << RCC_PLLCFGR_PLLR_Pos);     /* R = 2 (raw 0) */
    reg |= RCC_PLLCFGR_PLLREN;
    RCC->PLLCFGR = reg;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0x3u << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 0x3u) { }

    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }
}

static void uart_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);

    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (0xCu << GPIO_AFRL_AFSEL2_Pos) |
                     (0xCu << GPIO_AFRL_AFSEL3_Pos);

    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    (void)RCC->APB1ENR2;

    LPUART1->CR1 = 0;
    LPUART1->BRR = (uint32_t)(((uint64_t)256u * 170000000u) / 115200u);
    LPUART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((LPUART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

void board_init(void)
{
    SCB->CPACR |= (0xFu << 20);
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(170000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 170000000u;
}

const char *board_name(void)
{
    return "NUCLEO-G474RE";
}

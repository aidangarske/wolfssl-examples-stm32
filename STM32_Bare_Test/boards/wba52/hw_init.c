/* hw_init.c - STM32WBA52CG (NUCLEO-WBA52CG), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-WBA52CG:
 *   - HSI16 (16 MHz) as SYSCLK -- no PLL bring-up in this first cut
 *   - USART1 on PB12 (TX) / PA8 (RX) AF7, 115200 8N1, ST-LINK VCP
 *   - HSI48 (HSI 48 MHz internal) enabled for RNG kernel clock
 *
 * WBA52 has TinyAES + HASH + RNG + PKA + SAES (V2 layout). Single Cortex-M33
 * core (no M0+ side like WB55), AHB2-mapped peripherals.
 */

#include "stm32wbaxx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART1 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART1->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* HSI16 -> PLL1 -> SYSCLK = 100 MHz.
 *   HSI16 / M=1 -> 16 MHz ref -> *N=25 -> 400 MHz VCO -> /R=4 -> 100 MHz
 * VOS Range 1 + FLASH 3 WS for 100 MHz. Pattern follows STM32CubeFW WBA's
 * SystemClock_Config (Projects/STM32WBA55G-DK1/Examples_LL/RCC/...). */
#define WBA_PLL1RGE_8_16  (RCC_PLL1CFGR_PLL1RGE_0 | RCC_PLL1CFGR_PLL1RGE_1)
#define WBA_PLL1SRC_HSI   RCC_PLL1CFGR_PLL1SRC_1

static void clock_init(void)
{
    /* 1) Enable PWR clock so we can write VOSR */
    RCC->AHB4ENR |= RCC_AHB4ENR_PWREN;
    (void)RCC->AHB4ENR;

    /* 2) VOS Range 1 (high performance, allows HCLK > 16 MHz) */
    PWR->VOSR |= PWR_VOSR_VOS;
    while ((PWR->VOSR & PWR_VOSR_VOSRDY) == 0u) { }

    /* 3) FLASH 3 WS for 100 MHz HCLK */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | FLASH_ACR_LATENCY_3;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_3) { }

    /* 4) Confirm HSI16 is on (it's the reset default). */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* 5) Make sure SYSCLK is HSI16 (SW=00) before reconfiguring PLL */
    RCC->CFGR1 &= ~RCC_CFGR1_SW_Msk;
    while ((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) != 0u) { }

    /* 6) Disable PLL1 if running, then configure */
    RCC->CR &= ~RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) != 0u) { }

    /* PLL1CFGR: SRC=HSI, M=1 (raw 0), RGE=8-16 MHz, enable R output */
    RCC->PLL1CFGR = WBA_PLL1SRC_HSI |
                    (0u << RCC_PLL1CFGR_PLL1M_Pos) |
                    WBA_PLL1RGE_8_16 |
                    RCC_PLL1CFGR_PLL1REN;

    /* PLL1DIVR: N=25 (raw 24), R=4 (raw 3) */
    RCC->PLL1DIVR = ((25u - 1u) << RCC_PLL1DIVR_PLL1N_Pos) |
                    ((4u  - 1u) << RCC_PLL1DIVR_PLL1R_Pos);

    /* 7) Enable PLL1, wait for lock */
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0u) { }

    /* 8) Switch SYSCLK to PLL1R (SW=11) */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW_Msk) | RCC_CFGR1_SW_Msk;
    while ((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) != RCC_CFGR1_SWS_Msk) { }

    /* 9) Route RNG kernel clock from HSI16. (RNGSEL: 00=LSE, 01=LSI,
     *    10=HSI16, 11=PLL1Q.) HSI16 is always on, simplest path. */
    RCC->CCIPR2 = (RCC->CCIPR2 & ~RCC_CCIPR2_RNGSEL_Msk) |
                  RCC_CCIPR2_RNGSEL_1;
}

/* ---- USART1 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOA + GPIOB on AHB2 (positions per WBA RCC). */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    /* PB12 -> AF7 (USART1_TX) */
    GPIOB->MODER &= ~GPIO_MODER_MODE12_Msk;
    GPIOB->MODER |= (2u << GPIO_MODER_MODE12_Pos);
    GPIOB->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED12_Pos);
    GPIOB->AFR[1] &= ~(0xFu << GPIO_AFRH_AFSEL12_Pos);
    GPIOB->AFR[1] |= (7u << GPIO_AFRH_AFSEL12_Pos);

    /* PA8 -> AF7 (USART1_RX) */
    GPIOA->MODER &= ~GPIO_MODER_MODE8_Msk;
    GPIOA->MODER |= (2u << GPIO_MODER_MODE8_Pos);
    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED8_Pos);
    GPIOA->AFR[1] &= ~(0xFu << GPIO_AFRH_AFSEL8_Pos);
    GPIOA->AFR[1] |= (7u << GPIO_AFRH_AFSEL8_Pos);

    /* Enable USART1 clock (APB2ENR1 bit USART1EN) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* USART1: 8N1, oversample 16. PCLK2 = 100 MHz post-PLL. */
    USART1->CR1 = 0;
    USART1->BRR = 100000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M33F) */
    SCB->CPACR |= (0xFu << 20);
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(100000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 100000000u;
}


const char *board_name(void)
{
    return "NUCLEO-WBA52CG";
}

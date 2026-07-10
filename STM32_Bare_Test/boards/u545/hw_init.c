/* hw_init.c - STM32U545RE (NUCLEO-U545RE-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-U545RE-Q:
 *   - MSI 4 MHz at reset -> HSI 16 MHz -> PLL1 -> 96 MHz SYSCLK at
 *     VOS Range 1 (no EPOD booster). HSI16/M=1, N=12, /R=2 -> 192 MHz
 *     VCO -> 96 MHz SYSCLK. Range 1 supports up to 100 MHz without the
 *     EPOD booster, matching the proven path validated on B-U585I-IOT02A.
 *   - USART1 on PA9 (TX) / PA10 (RX) AF7, 115200 8N1, ST-LINK V3 VCP
 *   - HSI48 enabled for RNG kernel clock
 *
 * U545 has TinyAES + HASH + RNG + SAES + PKA (V2 layout) - same crypto
 * IP set as U585, just on the lower-power U5 sub-family.
 */

#include "stm32u5xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART1 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART1->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* HSI16 -> PLL1 -> 96 MHz SYSCLK at VOS Range 1 (no booster).
 *   HSI16 / M=1 -> 16 MHz ref (PLL1RGE = 8-16 MHz)
 *   N=12 -> VCO = 192 MHz (must be 128-544 MHz: OK)
 *   /R=2 -> 96 MHz SYSCLK
 * VOS Range 1 supports up to 100 MHz without EPOD; FLASH 3 WS @ 96 MHz. */
static void clock_init(void)
{
    /* 1) Enable PWR clock so we can write VOSR */
    RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
    (void)RCC->AHB3ENR;

    /* 2) Set VOS Range 1 (high perf, up to 100 MHz without booster) */
    PWR->VOSR = (PWR->VOSR & ~PWR_VOSR_VOS_Msk) | PWR_VOSR_VOS;
    while ((PWR->VOSR & PWR_VOSR_VOSRDY) == 0u) { }

    /* 3) FLASH 3 WS for 96 MHz at VOS Range 1 (RM0456 Table 17) */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) |
                 FLASH_ACR_LATENCY_3WS;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_3WS) { }

    /* 4) Enable HSI 16 MHz */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* 5) Make sure SYSCLK is on MSI/HSI (not PLL) before reconfiguring PLL */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW_Msk) | RCC_CFGR1_SW_0;
    while (((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos) != 1u) { }

    /* 6) Disable PLL1 if running, then configure */
    RCC->CR &= ~RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) != 0u) { }

    /* 7) PLL1CFGR: SRC=HSI16 (10b), M=1 (raw 0), RGE=8-16 MHz, R-output enabled.
     *    PLL1SRC field: 00=none, 01=MSI, 10=HSI, 11=HSE -> use both bits ?
     *    Actually PLL1SRC_1 alone = 0b10 = HSI16. */
    RCC->PLL1CFGR = RCC_PLL1CFGR_PLL1SRC_1 |              /* SRC=HSI16 */
                    (0u << RCC_PLL1CFGR_PLL1M_Pos) |      /* M=1 -> raw 0 */
                    RCC_PLL1CFGR_PLL1RGE_0 |              /* 8-16 MHz range */
                    RCC_PLL1CFGR_PLL1REN;                 /* enable R output */

    /* 8) PLL1DIVR: N=12 (raw 11), R=2 (raw 1).  Q+P left default. */
    RCC->PLL1DIVR = ((12u - 1u) << RCC_PLL1DIVR_PLL1N_Pos) |
                    ((2u  - 1u) << RCC_PLL1DIVR_PLL1R_Pos);

    /* 9) Enable PLL1, wait for lock */
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0u) { }

    /* 10) Switch SYSCLK to PLL1R (SW=11) */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW_Msk) | RCC_CFGR1_SW_Msk;
    while ((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) != RCC_CFGR1_SWS_Msk) { }

    /* 11) HSI48 - RNG kernel clock. Default RNGSEL on U5 is HSI48 (00). */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }
}

/* ---- USART1 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOA clock for PA9/PA10 */
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
    (void)RCC->AHB2ENR1;

    /* PA9 (TX), PA10 (RX): MODER=AF (10b), AF7 (USART1) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE9_Msk | GPIO_MODER_MODE10_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE9_Pos) | (2u << GPIO_MODER_MODE10_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED9_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED10_Pos);

    GPIOA->AFR[1] &= ~((0xFu << GPIO_AFRH_AFSEL9_Pos) |
                       (0xFu << GPIO_AFRH_AFSEL10_Pos));
    GPIOA->AFR[1] |= (7u << GPIO_AFRH_AFSEL9_Pos) |
                     (7u << GPIO_AFRH_AFSEL10_Pos);

    /* Enable USART1 clock (APB2 ENR; bit USART1EN) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* USART1: 8N1, oversample 16. PCLK2 = 96 MHz post-PLL (no APB2
     * prescaler set -> AHB = HCLK = SYSCLK = 96 MHz). */
    USART1->CR1 = 0;
    USART1->BRR = 96000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
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
    board_common_systick_init(96000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 96000000u;
}


const char *board_name(void)
{
    return "NUCLEO-U545RE-Q";
}

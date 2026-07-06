/* hw_init.c - STM32G491RE (NUCLEO-G491RE), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-G491RE:
 *   - HSI16 (16 MHz) as SYSCLK -- no PLL bring-up in this first cut
 *   - LPUART1 on PA2 (TX) / PA3 (RX) AF12, 115200 8N1, ST-LINK VCP
 *   - HSI48 enabled for RNG kernel clock
 *
 * G491 has TinyAES, RNG, PKA on AHB2; no HASH peripheral.
 */

#include "stm32g4xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over LPUART1 ------------------------------------- */
void board_putc(int ch)
{
    while ((LPUART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    LPUART1->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* Bring SYSCLK up to 170 MHz from HSI16 -> PLL.
 *   HSI16 / M=4 -> 4 MHz ref -> *N=85 -> 340 MHz VCO -> /R=2 -> 170 MHz
 * VOS Range1 boost mode (PWR_CR5.R1MODE=0) is required for >150 MHz on
 * G4 (RM0440 Section 6.1.4). FLASH 4 WS at 170 MHz with prefetch +
 * I/D-cache (RM0440 Table 9). */
static void clock_init(void)
{
    uint32_t reg;

    /* 1) Enable PWR clock to allow VOS / R1MODE writes */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    /* 2) Range 1 -> Boost mode: required for HCLK > 150 MHz */
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | PWR_CR1_VOS_0; /* Range 1 */
    while ((PWR->SR2 & PWR_SR2_VOSF) != 0u) { }
    PWR->CR5 &= ~PWR_CR5_R1MODE; /* 0 = boost */

    /* 3) Confirm HSI16 is on (it is at reset, but be explicit) */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* 4) FLASH 4 WS, prefetch + I-cache + D-cache for 170 MHz */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk)
               | (4u << FLASH_ACR_LATENCY_Pos)
               | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while (((FLASH->ACR & FLASH_ACR_LATENCY_Msk) >> FLASH_ACR_LATENCY_Pos) != 4u) { }

    /* 5) Make sure PLL is OFF before reconfiguring */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* 6) Configure PLL: SRC=HSI16, M=4 (raw 3), N=85, R=2 (raw 0).
     *    Enable PLL R output for SYSCLK. */
    reg = 0u;
    reg |= (0x2u << RCC_PLLCFGR_PLLSRC_Pos);   /* HSI16 */
    reg |= (3u   << RCC_PLLCFGR_PLLM_Pos);     /* M = 4 (raw 3) */
    reg |= (85u  << RCC_PLLCFGR_PLLN_Pos);     /* N = 85 */
    reg |= (0u   << RCC_PLLCFGR_PLLR_Pos);     /* R = 2 (raw 0) */
    reg |= RCC_PLLCFGR_PLLREN;
    RCC->PLLCFGR = reg;

    /* 7) Turn PLL on, wait for lock, switch SYSCLK to PLL R clock */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0x3u << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 0x3u) { }

    /* 8) Enable HSI48 -- RNG kernel clock source (RNGSEL=00, CCIPR.CLK48SEL
     *    = 00 -> HSI48). Without it the RNG never produces DRDY. */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }
}

/* ---- LPUART1 init ----------------------------------------------------- */
static void uart_init(void)
{
    /* Enable GPIOA clock (AHB2ENR bit GPIOAEN) */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    /* PA2 (TX), PA3 (RX): MODER = AF (10b), AF12 (LPUART1) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);

    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);

    /* AFRL: PA2 -> AFR[0] bits[11:8]; PA3 -> AFR[0] bits[15:12]; AF12 = 0xC */
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (0xCu << GPIO_AFRL_AFSEL2_Pos) |
                     (0xCu << GPIO_AFRL_AFSEL3_Pos);

    /* Enable LPUART1 clock (APB1ENR2 bit LPUART1EN) */
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    (void)RCC->APB1ENR2;

    /* LPUART1 kernel clock = PCLK1 = HCLK = 170 MHz post-PLL.
     * LPUART_BRR = (256 * f_LPUART) / baud. Min BRR is 0x300 (RM0440).
     * Use uint64 to avoid overflow: 256 * 170000000 > 2^32. */
    LPUART1->CR1 = 0;
    LPUART1->BRR = (uint32_t)(((uint64_t)256u * 170000000u) / 115200u);
    LPUART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((LPUART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M4F) */
    SCB->CPACR |= (0xFu << 20);
    /* Disable FPU lazy stacking (LSPEN=0) -- avoids LSPERR */
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
    return "NUCLEO-G491RE";
}

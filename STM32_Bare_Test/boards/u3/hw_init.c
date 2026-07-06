/* hw_init.c - STM32U385RG (NUCLEO-U385RG-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-U385RG-Q:
 *   - SYSCLK = MSIS RC0 96 MHz (U3 max) via the EPOD booster at VOS range 1.
 *     SYSCLK is MSIS at reset, so retuning MSIS to RC0/DIV1 brings the core
 *     straight to 96 MHz (no SYSCLK source switch). Mirrors ST's U385 example
 *     SystemClock_Config (SMPS + EPOD booster(MSIS,DIV1) + VOS1 + 2 WS).
 *   - USART1 on PA9 (TX) / PA10 (RX) AF7, 115200 8N1, ST-LINK VCP
 *   - HSI48 enabled for RNG kernel clock
 *
 * No HAL drivers are pulled in. All state lives directly in MMIO.
 */

#include "stm32u3xx.h"
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
/* Bring SYSCLK to 96 MHz (U3 max). MSIS is the reset SYSCLK source, so this
 * retunes MSIS to RC0 (96 MHz) after enabling the EPOD booster (mandatory for
 * MSIS above 48 MHz) at VOS range 1 with 2 flash wait states. Order matches
 * ST's NUCLEO-U385RG-Q example: supply -> EPOD clk src -> EPOD enable -> VOS1
 * -> flash WS -> MSIS RC0. */
#define U3_SYSCLK_HZ  96000000u
static void clock_init(void)
{
    /* PWR clock for VOSR access (U3: AHB1ENR2.PWREN). */
    RCC->AHB1ENR2 |= RCC_AHB1ENR2_PWREN;
    (void)RCC->AHB1ENR2;

    /* Keep the default LDO supply -- SMPS is a power-efficiency choice, not a
     * 96 MHz requirement, and switching it (PWR_CR3_REGSEL) depends on board
     * supply wiring; VOS1 + EPOD booster reach 96 MHz on the LDO. */

    /* EPOD booster clock source = MSIS, divider 1 (BOOSTSEL_0, BOOSTDIV=0). */
    RCC->CFGR4 = (RCC->CFGR4 & ~(RCC_CFGR4_BOOSTDIV | RCC_CFGR4_BOOSTSEL)) |
                 RCC_CFGR4_BOOSTSEL_0;

    /* Enable the EPOD booster, wait until ready. */
    PWR->VOSR |= PWR_VOSR_BOOSTEN;
    while ((PWR->VOSR & PWR_VOSR_BOOSTRDY) == 0u) { }

    /* VOS range 1 (high performance, up to 96 MHz). */
    PWR->VOSR = (PWR->VOSR & ~(PWR_VOSR_R1EN | PWR_VOSR_R2EN)) | PWR_VOSR_R1EN;
    while ((PWR->VOSR & PWR_VOSR_R1RDY) == 0u) { }

    /* Flash 2 wait states for 96 MHz at VOS range 1. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | FLASH_ACR_LATENCY_2;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != FLASH_ACR_LATENCY_2) { }

    /* Retune MSIS to RC0 (96 MHz) / DIV1; MSIRGSEL takes the range from
     * ICSCR1. SYSCLK stays on MSIS, so the core jumps to 96 MHz here. */
    RCC->ICSCR1 = (RCC->ICSCR1 & ~(RCC_ICSCR1_MSISSEL | RCC_ICSCR1_MSISDIV)) |
                  RCC_ICSCR1_MSIRGSEL;
    RCC->CR |= RCC_CR_MSISON;
    while ((RCC->CR & RCC_CR_MSISRDY) == 0u) { }

    /* Enable HSI48 -- RNG kernel clock on U3. Without it the RNG never
     * produces DRDY and wc_GenerateSeed spins forever. */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }
}

/* ---- USART1 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOA clock for PA9/PA10 */
    RCC->AHB2ENR1 |= RCC_AHB2ENR1_GPIOAEN;
    (void)RCC->AHB2ENR1;

    /* PA9 (TX), PA10 (RX): MODER = AF (10b), AF7 (USART1) */
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

    /* USART1: 8N1, oversampling 16. PCLK2 = HCLK = SYSCLK = 96 MHz. */
    USART1->CR1 = 0;
    USART1->BRR = U3_SYSCLK_HZ / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Force-enable FPU CP10/CP11 full access. */
    SCB->CPACR |= (0xFu << 20);
    /* Disable FPU lazy stacking (LSPEN=0) -- avoids LSPERR on H5/U5/U3 */
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();   /* CMSIS template -- vector table + default clocks */
    clock_init();
    uart_init();
    board_common_systick_init(U3_SYSCLK_HZ);
}

uint32_t board_sysclk_hz(void)
{
    return U3_SYSCLK_HZ;
}


const char *board_name(void)
{
    return "NUCLEO-U385RG-Q";
}

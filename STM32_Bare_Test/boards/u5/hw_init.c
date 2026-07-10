/* hw_init.c - STM32U575ZI (NUCLEO-U575ZI-Q), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-U575ZI-Q:
 *   - MSI 4 MHz at reset, then switched to HSI 16 MHz SYSCLK (no PLL) in
 *     clock_init(); board_sysclk_hz() returns 16 MHz.
 *   - USART1 on PA9 (TX) / PA10 (RX) AF7, 115200 8N1, ST-LINK VCP
 *   - HSI48 enabled for RNG kernel clock
 *
 * No HAL drivers are pulled in. All state lives directly in MMIO.
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
static void clock_init(void)
{
    /* Switch SYSCLK from MSI (4 MHz) to HSI (16 MHz). Default VOS4
     * supports up to 25 MHz so no VOS bump needed. 4x speedup gets ECC
     * KAT to finish in reasonable time. PLL bring-up to 160 MHz is a
     * follow-up (see wolfboot/hal/stm32u5.c clock_pll_on for reference). */

    /* Enable HSI 16 MHz */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* Flash latency: 1WS for 16 MHz at VOS4 (datasheet table) */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) |
                 (1u << FLASH_ACR_LATENCY_Pos);

    /* Switch SYSCLK source to HSI (CFGR1.SW = 01 = HSI16). The CMSIS macro
     * RCC_CFGR1_SW_0 = bit 0 of the field = value 0b01 (HSI). RCC_CFGR1_SW_1
     * is bit 1 = value 0b10 = HSE -- do NOT use that. */
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SW_Msk) | RCC_CFGR1_SW_0;
    while (((RCC->CFGR1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos) != 1u) { }

    /* Enable HSI48 -- required as the RNG kernel clock on U5. Without it,
     * the RNG never produces DRDY and wc_GenerateSeed spins forever. */
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

    /* USART1: 8N1, oversampling 16. PCLK2 = 4 MHz; BRR = PCLK / baud. */
    USART1->CR1 = 0;
    USART1->BRR = 16000000u / 115200u;
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
    /* Disable FPU lazy stacking (LSPEN=0) -- avoids LSPERR on H5/U5 */
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();   /* CMSIS template -- vector table + default clocks */
    clock_init();
    uart_init();
    board_common_systick_init(16000000u); /* HSI 16 MHz */
}

uint32_t board_sysclk_hz(void)
{
    return 16000000u;
}


const char *board_name(void)
{
    return "NUCLEO-U575ZI-Q";
}

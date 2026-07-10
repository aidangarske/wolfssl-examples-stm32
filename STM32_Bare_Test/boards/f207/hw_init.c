/* hw_init.c - STM32F207ZG (NUCLEO-F207ZG), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-F207ZG:
 *   - HSI 16 MHz at reset, then PLL to 120 MHz SYSCLK (F207 max) in
 *     clock_init(); board_sysclk_hz() returns 120 MHz.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK V2-1 VCP on the
 *     NUCLEO-F207ZG (same Nucleo-144 layout as F4/F7 boards).
 *   - Cortex-M3, no FPU.
 *
 * F207 silicon HW crypto: RNG only. F215/F217 have CRYP + HASH; F207
 * (and F205) do not.
 */

#include "stm32f2xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART3->SR & USART_SR_TXE) == 0) {
        /* wait */
    }
    USART3->DR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
static void clock_init(void)
{
    /* HSI 16 MHz -> PLL -> 120 MHz SYSCLK; PLL48 = 48 MHz (kernel clock
     * for the RNG IP -- the F2 RNG silicon requires PLL48CLK and will
     * report a seed error indefinitely without it).
     *   PLLM = 8   (VCO_in  = 16/8 = 2 MHz)
     *   PLLN = 120 (VCO_out = 2*120 = 240 MHz, within F2 192-432 range)
     *   PLLP = /2  (SYSCLK  = 240/2 = 120 MHz)
     *   PLLQ = /5  (PLL48   = 240/5 = 48 MHz)
     */

    /* Enable PWR clock and stay at Scale 1 default (single-VOS bit on F2). */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;

    /* Flash latency = 3 WS for 120 MHz at 2.7-3.6 V (RM0033 Section 3.5.1).
     * Enable prefetch + I-cache + D-cache. */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_3WS;

    /* HSI is on by default. Configure PLL while it's off. */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }

    /* PLLCFGR: PLLSRC=HSI (bit 22 = 0), PLLM=8, PLLN=120, PLLP=/2 (=0),
     * PLLQ=5. */
    RCC->PLLCFGR = (8u   << RCC_PLLCFGR_PLLM_Pos) |
                   (120u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u   << RCC_PLLCFGR_PLLP_Pos) |
                   (5u   << RCC_PLLCFGR_PLLQ_Pos);
    /* PLLSRC bit defaults to HSI (0). */

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) { }

    /* AHB/1 (120 MHz HCLK), APB1/4 (=30 MHz, max 30), APB2/2 (=60 MHz, max 60) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* Switch SYSCLK to PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
}

/* ---- USART3 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOD clock for PD8/PD9 (AHB1ENR bit 3) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;

    /* PD8 (TX), PD9 (RX): MODER = AF (10b), AF7 (USART3) */
    GPIOD->MODER &= ~(GPIO_MODER_MODER8_Msk | GPIO_MODER_MODER9_Msk);
    GPIOD->MODER |= (2u << GPIO_MODER_MODER8_Pos) | (2u << GPIO_MODER_MODER9_Pos);

    /* F2 CMSIS only exposes the full-mask `GPIO_OSPEEDR_OSPEED8`
     * symbol (no `_Pos`); set high-speed on PD8/PD9 via the masks. */
    GPIOD->OSPEEDR |= GPIO_OSPEEDR_OSPEED8 | GPIO_OSPEEDR_OSPEED9;

    GPIOD->AFR[1] &= ~((0xFu << ((8 - 8) * 4)) | (0xFu << ((9 - 8) * 4)));
    GPIOD->AFR[1] |= (7u << ((8 - 8) * 4)) | (7u << ((9 - 8) * 4));

    /* Enable USART3 clock (APB1ENR; bit USART3EN) */
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    /* USART3: 8N1, oversampling 16. PCLK1 = HCLK/4 = 30 MHz post-PLL. */
    USART3->CR1 = 0;
    USART3->BRR = 30000000u / 115200u;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Cortex-M3 -- no FPU, skip CPACR write. */
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(120000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 120000000u;
}


const char *board_name(void)
{
    return "NUCLEO-F207ZG";
}

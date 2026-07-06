/* hw_init.c - STM32F439ZI (NUCLEO-F439ZI), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-F439ZI:
 *   - HSI 16 MHz at reset; keep it as SYSCLK (no PLL setup) for bring-up
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7, 115200 8N1, ST-LINK VCP
 *
 * No HAL drivers are pulled in.
 */

#include "stm32f4xx.h"
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
    /* HSI 16 MHz -> PLL -> 144 MHz SYSCLK; PLL48 = 48 MHz (for RNG).
     *   PLLM = 8   (VCO_in  = 16/8 = 2 MHz)
     *   PLLN = 144 (VCO_out = 2*144 = 288 MHz)
     *   PLLP = /2  (SYSCLK  = 288/2 = 144 MHz)
     *   PLLQ = /6  (PLL48   = 288/6 = 48 MHz)
     */

    /* Enable PWR clock and set voltage scale 1 (required for >144 MHz HSE,
     * but safer to set anyway). */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;  /* Scale 1 */

    /* Flash latency = 4 WS for 144 MHz at 3.3V */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_4WS;

    /* HSI is on by default. Configure PLL while it's off. */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }

    /* PLLCFGR: PLLSRC=HSI, PLLM=8, PLLN=144, PLLP=2 (encoded as 0), PLLQ=6 */
    RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos) |
                   (144u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u << RCC_PLLCFGR_PLLP_Pos) |     /* /2 */
                   (6u << RCC_PLLCFGR_PLLQ_Pos);
    /* PLLSRC = HSI (default 0) */

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) { }

    /* AHB/1, APB1/4 (=36 MHz), APB2/2 (=72 MHz) */
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

    GPIOD->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED8_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED9_Pos);

    GPIOD->AFR[1] &= ~((0xFu << ((8 - 8) * 4)) | (0xFu << ((9 - 8) * 4)));
    GPIOD->AFR[1] |= (7u << ((8 - 8) * 4)) | (7u << ((9 - 8) * 4));

    /* Enable USART3 clock (APB1 ENR; bit USART3EN) */
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    /* USART3: 8N1, oversampling 16. PCLK1 = 36 MHz (144/4); BRR = PCLK/baud */
    USART3->CR1 = 0;
    USART3->BRR = 36000000u / 115200u;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Force-enable FPU CP10/CP11 full access (Cortex-M4F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(144000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 144000000u;
}


const char *board_name(void)
{
    return "NUCLEO-F439ZI";
}

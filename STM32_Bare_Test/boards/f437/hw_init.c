/* hw_init.c - STM32F437 (STM32437I-EVAL), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for STM32437I-EVAL:
 *   - HSI 16 MHz -> PLL -> 144 MHz SYSCLK; PLL48 = 48 MHz (RNG kernel clock)
 *   - UART4 on PC10 (TX) / PC11 (RX) AF8, 115200 8N1, ST-LINK VCP route
 *     on STM32437I-EVAL (vs USART3 PD8/PD9 on the F439 NUCLEO).
 *
 * F437 silicon is otherwise register-compatible with F439 from this
 * driver's point of view (same CRYP+HASH+RNG IP, same RCC layout, same
 * flash latency table). No HAL drivers are pulled in.
 */

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over UART4 --------------------------------------- */
void board_putc(int ch)
{
    while ((UART4->SR & USART_SR_TXE) == 0) {
        /* wait */
    }
    UART4->DR = (uint32_t)ch & 0xFFu;
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

    /* Enable PWR clock and set voltage scale 1. */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;  /* Scale 1 */

    /* Flash latency = 4 WS for 144 MHz at 3.3V */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN |
                 FLASH_ACR_LATENCY_4WS;

    /* HSI on by default. Configure PLL while it's off. */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) { }

    /* PLLCFGR: PLLSRC=HSI, PLLM=8, PLLN=144, PLLP=2 (encoded as 0), PLLQ=6 */
    RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos) |
                   (144u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u << RCC_PLLCFGR_PLLP_Pos) |     /* /2 */
                   (6u << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) { }

    /* AHB/1, APB1/4 (=36 MHz), APB2/2 (=72 MHz) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* Switch SYSCLK to PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
}

/* ---- UART4 init: PC10 (TX) / PC11 (RX) AF8 (STM32437I-EVAL VCP) ------- */
static void uart_init(void)
{
    /* Enable GPIOC clock for PC10/PC11 (AHB1ENR bit 2) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;

    /* PC10 (TX), PC11 (RX): MODER = AF (10b), AF8 (UART4) */
    GPIOC->MODER &= ~(GPIO_MODER_MODER10_Msk | GPIO_MODER_MODER11_Msk);
    GPIOC->MODER |= (2u << GPIO_MODER_MODER10_Pos) |
                    (2u << GPIO_MODER_MODER11_Pos);

    GPIOC->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED10_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED11_Pos);

    /* AFRH covers pins 8..15; PC10 = AFRH[10-8]=AFRH[2], PC11 = AFRH[3]. */
    GPIOC->AFR[1] &= ~((0xFu << ((10 - 8) * 4)) | (0xFu << ((11 - 8) * 4)));
    GPIOC->AFR[1] |= (8u << ((10 - 8) * 4)) | (8u << ((11 - 8) * 4));

    /* Enable UART4 clock (APB1 ENR; bit UART4EN) */
    RCC->APB1ENR |= RCC_APB1ENR_UART4EN;
    (void)RCC->APB1ENR;

    /* UART4: 8N1, oversampling 16. PCLK1 = 36 MHz; BRR = PCLK/baud */
    UART4->CR1 = 0;
    UART4->BRR = 36000000u / 115200u;
    UART4->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
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
    return "STM32437I-EVAL";
}

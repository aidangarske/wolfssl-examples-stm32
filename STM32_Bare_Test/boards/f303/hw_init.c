/* hw_init.c - NUCLEO-F303ZE (STM32F303ZE), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-F303ZE:
 *   - HSI 8 MHz -> PLL (HSI/2 * 16) -> 64 MHz SYSCLK. No PLL Q output
 *     because the F303 PLL has no Q divider and F303xE has no RNG to
 *     feed anyway.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7, 115200 8N1, routed to the
 *     on-board ST-LINK V2-1 VCP. Standard NUCLEO-144 pinout (same as
 *     F439ZI, F767ZI, H753ZI, H563ZI).
 *
 * STM32F303xE silicon has NO HW crypto -- no CRYP, no HASH, no TRNG.
 * wolfCrypt runs in pure software (or thumb2 inline-asm under
 * CONFIG=asm). This board is in the harness as a Cortex-M4F software
 * reference point.
 */

#include "stm32f303xe.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART3->ISR & USART_ISR_TXE) == 0) {
        /* wait for TX register empty */
    }
    USART3->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* HSI 8 MHz internal -> PLL -> 64 MHz SYSCLK.
 *   PLLSRC = HSI/2 (CFGR.PLLSRC = 0)
 *   PLLMUL = x16 (CFGR.PLLMUL = 0b1110)
 *   VCO_in  = 8 / 2 = 4 MHz
 *   VCO_out = 4 * 16 = 64 MHz
 * Flash 2 WS at 64 MHz (per RM0316 Table 11). */
static void clock_init(void)
{
    /* HSI on (default at reset) */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* Flash 2 WS + prefetch */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_1;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_1) { }

    /* Make sure SYSCLK is on HSI before reconfiguring PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW);
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) { }

    /* PLL off */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* CFGR: PLLSRC = HSI/2 (= 0), PLLMUL = x16 (= 0b1110) */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL)) |
                RCC_CFGR_PLLMUL16;

    /* AHB/1 (= 64 MHz), APB1/2 (= 32 MHz, max 36), APB2/1 (= 64 MHz). */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                               RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 |
                RCC_CFGR_PPRE2_DIV1;

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    /* Switch SYSCLK to PLL (CFGR.SW = 10b) */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
}

/* ---- USART3 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOD clock on AHB1 (bit 20 for F303) */
    RCC->AHBENR |= RCC_AHBENR_GPIODEN;
    (void)RCC->AHBENR;

    /* PD8 (TX), PD9 (RX): MODER = AF (10b), AF7 (USART3) */
    GPIOD->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9);
    GPIOD->MODER |= (2u << GPIO_MODER_MODER8_Pos) |
                    (2u << GPIO_MODER_MODER9_Pos);
    GPIOD->OSPEEDR |= (3u << GPIO_OSPEEDER_OSPEEDR8_Pos) |
                      (3u << GPIO_OSPEEDER_OSPEEDR9_Pos);
    GPIOD->AFR[1] &= ~((0xFu << GPIO_AFRH_AFRH0_Pos) |
                       (0xFu << GPIO_AFRH_AFRH1_Pos));
    GPIOD->AFR[1] |= (7u << GPIO_AFRH_AFRH0_Pos) |
                     (7u << GPIO_AFRH_AFRH1_Pos);

    /* Enable USART3 clock on APB1 */
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    /* USART3: 8N1, oversample 16. PCLK1 = 32 MHz post-PLL (APB1 /2).
     * BRR = PCLK / baud (oversample 16 is the default OVER8=0). */
    USART3->CR1 = 0;
    USART3->BRR = 32000000u / 115200u;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART3->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M4F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(64000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 64000000u;
}


const char *board_name(void)
{
    return "NUCLEO-F303ZE";
}

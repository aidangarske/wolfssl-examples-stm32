/* hw_init.c - STM32H753ZI (NUCLEO-H753ZI), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-H753ZI:
 *   - HSI 64 MHz at reset, then PLL1 to 480 MHz SYSCLK (H7 max) in
 *     clock_init(); board_sysclk_hz() returns 480 MHz.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK VCP. Same pins as F439.
 */

#include "stm32h7xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    USART3->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* Bring SYSCLK up to 480 MHz from HSE 8 MHz BYPASS (ST-LINK MCO). NUCLEO-
 * H743ZI/H753ZI default solder bridges connect MCO to OSC_IN, so HSEBYP
 * works with no rework. HCLK = 240 MHz, PCLK1/PCLK2/PCLK3/PCLK4 = 120 MHz.
 *
 * Path: HSE(8) -> /M=1 -> 8 MHz -> *N=120 -> VCO=960 MHz -> /P=2 = 480 MHz.
 * VOS Scale 1 + ODEN (overdrive) is required to license HCLK > 200 MHz on
 * H753 (RM0433 Section 6.6.1). Pattern follows wolfBoot's hal/stm32h7.c. */
#define H7_PWR_CR3_LDOEN              (1u << 1)
#define H7_PWR_CSR1_ACTVOSRDY         (1u << 13)
#define H7_PWR_D3CR_VOS_SCALE_1       0x3u
#define H7_PWR_D3CR_VOS_SHIFT         14u
#define H7_PWR_D3CR_VOSRDY            (1u << 13)
#define H7_SYSCFG_PWRCR_ODEN          (1u << 0)

static void clock_init(void)
{
    uint32_t reg;

    /* 1) Boost VOS to Scale 1 with overdrive (license to run > 200 MHz) */
    PWR->CR3 |= H7_PWR_CR3_LDOEN;
    while ((PWR->CSR1 & H7_PWR_CSR1_ACTVOSRDY) == 0u) { }

    PWR->D3CR |= (H7_PWR_D3CR_VOS_SCALE_1 << H7_PWR_D3CR_VOS_SHIFT);
    (void)PWR->D3CR;
    /* SYSCFG clock must be on before writing PWRCR.ODEN */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;
    SYSCFG->PWRCR |= H7_SYSCFG_PWRCR_ODEN;
    (void)SYSCFG->PWRCR;
    while ((PWR->D3CR & H7_PWR_D3CR_VOSRDY) == 0u) { }

    /* 2) Pre-set FLASH latency for 240 MHz HCLK at VOS1+OD: 4 WS,
     *    WRHIGHFREQ = 11b (225-240 MHz range, RM0433 Section 4.3.8) */
    FLASH->ACR = (FLASH->ACR & ~(FLASH_ACR_LATENCY_Msk | FLASH_ACR_WRHIGHFREQ_Msk))
               | (4u << FLASH_ACR_LATENCY_Pos)
               | (3u << FLASH_ACR_WRHIGHFREQ_Pos);
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != (4u << FLASH_ACR_LATENCY_Pos)) { }

    /* 3) Make sure HSI is on as the SYSCLK source while we reconfigure PLL */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0u << RCC_CFGR_SW_Pos);

    /* 4) Bring up HSE BYPASS (8 MHz from STLINK MCO on NUCLEO-H753ZI) */
    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    while ((RCC->CR & RCC_CR_HSERDY) == 0u) { }

    /* 5) AHB/APB prescalers BEFORE switching to PLL so we never overclock
     *    a peripheral domain mid-transition. HPRE=/2 (240 MHz HCLK),
     *    APBx PPRE=/2 (120 MHz). D1CPRE=/1. */
    RCC->D1CFGR = (RCC->D1CFGR & ~(RCC_D1CFGR_HPRE_Msk |
                                    RCC_D1CFGR_D1PPRE_Msk |
                                    RCC_D1CFGR_D1CPRE_Msk))
                | (0x8u << RCC_D1CFGR_HPRE_Pos)    /* /2  */
                | (0x4u << RCC_D1CFGR_D1PPRE_Pos)  /* /2  */
                | (0x0u << RCC_D1CFGR_D1CPRE_Pos); /* /1  */
    RCC->D2CFGR = (RCC->D2CFGR & ~(RCC_D2CFGR_D2PPRE1_Msk |
                                    RCC_D2CFGR_D2PPRE2_Msk))
                | (0x4u << RCC_D2CFGR_D2PPRE1_Pos)  /* /2 */
                | (0x4u << RCC_D2CFGR_D2PPRE2_Pos); /* /2 */
    RCC->D3CFGR = (RCC->D3CFGR & ~RCC_D3CFGR_D3PPRE_Msk)
                | (0x4u << RCC_D3CFGR_D3PPRE_Pos);  /* /2 */

    /* 6) PLL1 config: HSE source, M=1, N=120, P=2, Q=20, R=2.
     *    PLL1RGE = 8-16 MHz (input is 8 MHz after /M=1). VCOSEL=0 wide. */
    RCC->PLLCKSELR = (RCC->PLLCKSELR & ~(RCC_PLLCKSELR_PLLSRC_Msk |
                                          RCC_PLLCKSELR_DIVM1_Msk))
                   | (0x2u << RCC_PLLCKSELR_PLLSRC_Pos)   /* HSE */
                   | (0x1u << RCC_PLLCKSELR_DIVM1_Pos);    /* M=1 */

    RCC->PLL1DIVR = ((120u - 1u) << RCC_PLL1DIVR_N1_Pos)
                  | ((2u   - 1u) << RCC_PLL1DIVR_P1_Pos)
                  | ((20u  - 1u) << RCC_PLL1DIVR_Q1_Pos)
                  | ((2u   - 1u) << RCC_PLL1DIVR_R1_Pos);

    reg = RCC->PLLCFGR;
    reg &= ~(RCC_PLLCFGR_PLL1RGE_Msk | RCC_PLLCFGR_PLL1VCOSEL_Msk);
    reg |= (0x3u << RCC_PLLCFGR_PLL1RGE_Pos)  /* 8-16 MHz input */
        |  RCC_PLLCFGR_DIVP1EN
        |  RCC_PLLCFGR_DIVQ1EN
        |  RCC_PLLCFGR_DIVR1EN;
    RCC->PLLCFGR = reg;

    /* 7) Turn PLL1 on, wait, switch SYSCLK */
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0u) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0x3u << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 0x3u) { }

    /* 8) USART2/3/4/5/7/8 kernel clock = PCLK1 (USART234578SEL = 000).
     *    Reset default is supposed to be 000, but we hit a state where it
     *    was 010 (PLL3_Q), which is unset and produced corrupt UART. */
    RCC->D2CCIP2R &= ~RCC_D2CCIP2R_USART28SEL_Msk;

    /* 9) Enable HSI48 -- kept as RNG kernel clock source on H7. */
    RCC->CR |= RCC_CR_HSI48ON;
    while ((RCC->CR & RCC_CR_HSI48RDY) == 0u) { }
}

/* ---- USART3 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* GPIOD clock (AHB4 on H7) + USART3 clock (APB1 low).
     * USART234578SEL was forced to 000 (PCLK1) by clock_init() above so
     * PCLK1 = HCLK/2 = 120 MHz post-PLL is the kernel clock. */
    RCC->AHB4ENR  |= RCC_AHB4ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;
    (void)RCC->APB1LENR;

    /* PD8 (TX) / PD9 (RX) AF7, USART3 at 120 MHz kernel clock. */
    board_common_uart_pin_init(GPIOD, 8u, 9u, 7u);
    board_common_uart_basic_init(USART3, 120000000u, 115200u);
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M7F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    /* SysTick CLKSOURCE=1 -> CPU clock = 480 MHz */
    board_common_systick_init(480000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 480000000u;
}


const char *board_name(void)
{
    return "NUCLEO-H753ZI";
}

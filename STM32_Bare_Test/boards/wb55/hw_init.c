/* hw_init.c - STM32WB55RG (NUCLEO-WB55RG), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-WB55RG (M4 application core):
 *   - MSI 4 MHz default at reset; switch SYSCLK to HSI (16 MHz)
 *   - USART1 on PB6 (TX) / PB7 (RX) AF7, 115200 8N1, ST-LINK VCP
 *   - HSI48 enabled for RNG kernel clock (default RNGSEL/CLK48SEL = 0)
 *
 * Note: WB55 is a dual-core (M4 + M0+) wireless MCU. The M0+ side runs the
 * BLE/802.15.4 firmware. This bring-up touches only the M4 RCC tree and
 * leaves RCC->EXTCFGR / C2 register banks alone.
 */

#include "stm32wbxx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* ---- printf retarget over USART1 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE) == 0) {
        /* wait for TX FIFO space */
    }
    USART1->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init ------------------------------------------------------- */
/* Bring SYSCLK up to 64 MHz from HSI16 -> PLL.
 *   HSI16 / M=1 -> 16 MHz ref -> *N=8 -> 128 MHz VCO -> /R=2 -> 64 MHz
 * VOS Range1 (default at reset) supports HCLK <= 64 MHz on M4. Flash
 * latency at 64 MHz = 3 WS (RM0434 Section 3.3.3 Table 9). */
static void clock_init(void)
{
    uint32_t reg;

    /* 1) Enable HSI16 if not already on (it is at MSI/4 default but HSI is
     *    needed as PLL source). */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* 2) Pre-set FLASH latency to 3 WS so it's already correct when the
     *    SYSCLK switch happens. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) |
                 (3u << FLASH_ACR_LATENCY_Pos);
    while (((FLASH->ACR & FLASH_ACR_LATENCY_Msk) >> FLASH_ACR_LATENCY_Pos) != 3u) { }

    /* 3) Make sure PLL is OFF before reconfiguring. */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* 4) Configure PLL: SRC=HSI16, M=1, N=8, R=2 (PLLR encoded as N-1 = 1).
     *    Enable PLL R output for SYSCLK. P/Q outputs left disabled. */
    reg = 0u;
    reg |= (0x2u << RCC_PLLCFGR_PLLSRC_Pos);  /* HSI16 */
    reg |= (0x0u << RCC_PLLCFGR_PLLM_Pos);    /* M = 1 (raw 0) */
    reg |= (8u   << RCC_PLLCFGR_PLLN_Pos);    /* N = 8         */
    reg |= (1u   << RCC_PLLCFGR_PLLR_Pos);    /* R = 2 (raw 1) */
    reg |= RCC_PLLCFGR_PLLREN;
    RCC->PLLCFGR = reg;

    /* 5) Turn PLL on, wait, switch SYSCLK to PLL R clock (CFGR.SW = 11) */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | (0x3u << RCC_CFGR_SW_Pos);
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 0x3u) { }

    /* 6) Enable HSI48. Required as RNG kernel clock (default RNGSEL=00 ->
     *    CLK48 path; default CLK48SEL=00 -> HSI48). Without it the RNG
     *    never produces DRDY and wc_GenerateSeed spins forever. */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) == 0u) { }
}

/* ---- USART1 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOB clock (AHB2ENR bit GPIOBEN) */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;
    (void)RCC->AHB2ENR;

    /* PB6 (TX), PB7 (RX): MODER = AF (10b), AF7 (USART1) */
    GPIOB->MODER &= ~(GPIO_MODER_MODE6_Msk | GPIO_MODER_MODE7_Msk);
    GPIOB->MODER |= (2u << GPIO_MODER_MODE6_Pos) | (2u << GPIO_MODER_MODE7_Pos);

    GPIOB->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED6_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED7_Pos);

    GPIOB->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL6_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL7_Pos));
    GPIOB->AFR[0] |= (7u << GPIO_AFRL_AFSEL6_Pos) |
                     (7u << GPIO_AFRL_AFSEL7_Pos);

    /* Enable USART1 clock (APB2 ENR; bit USART1EN) */
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;

    /* USART1: 8N1, oversampling 16. PCLK2 = HCLK = 64 MHz post-PLL. */
    USART1->CR1 = 0;
    USART1->BRR = 64000000u / 115200u;
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* Force-enable FPU CP10/CP11 full access (Cortex-M4F) */
    SCB->CPACR |= (0xFu << 20);
    /* Disable FPU lazy stacking (LSPEN=0) -- avoids LSPERR */
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();   /* CMSIS template -- vector table + default clocks */
    clock_init();
    uart_init();
    board_common_systick_init(64000000u); /* PLL out: 64 MHz */
}

uint32_t board_sysclk_hz(void)
{
    return 64000000u;
}


const char *board_name(void)
{
    return "NUCLEO-WB55RG";
}

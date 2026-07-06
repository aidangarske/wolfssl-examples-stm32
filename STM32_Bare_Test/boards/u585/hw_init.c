/* hw_init.c - STM32U585AI (B-U585I-IOT02A), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for B-U585I-IOT02A Discovery board:
 *   - MSI 4 MHz at reset -> HSI 16 MHz -> PLL1 -> 96 MHz SYSCLK at
 *     VOS Range 1 (no EPOD booster). HSI16/M=1, N=12, /R=2 -> 192 MHz
 *     VCO -> 96 MHz SYSCLK. Range 1 supports up to 100 MHz without the
 *     EPOD booster; the 160 MHz path needs BOOSTEN+BOOSTRDY which doesn't
 *     assert on this Discovery board variant. 96 MHz is 6x HSI16 which
 *     is plenty to clear the multi-curve PKA quirk that hangs the ECC
 *     sweep at HSI16.
 *   - USART1 on PA9 (TX) / PA10 (RX) AF7, 115200 8N1, ST-LINK V3E VCP
 *   - HSI48 enabled for RNG kernel clock
 *
 * U585 has TinyAES + HASH + RNG + SAES + PKA (V2 layout).
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
/* HSI16 -> PLL1 -> 160 MHz SYSCLK (via EPOD booster) with graceful
 * fallback to 96 MHz if BOOSTRDY does not assert.
 *
 * 160 MHz path: VOS Range 1 + EPOD booster, HSI16 * N=20 / R=2 = 160,
 *               FLASH 4 WS.
 * 96 MHz path:  VOS Range 1 alone (no booster), HSI16 * N=12 / R=2 = 96,
 *               FLASH 3 WS.
 *
 * Earlier note in this file said BOOSTRDY does not assert on the
 * IOT02A. The HAL flow (HAL_PWREx_ControlVoltageScaling) sets VOS +
 * BOOSTEN together in a single MODIFY_REG and waits VOSRDY +
 * ACTVOSRDY before testing BOOSTRDY. Try that sequence with a
 * bounded wait; on failure we fall back to the safe 96 MHz config. */
#ifndef U585_BOOSTRDY_TIMEOUT
    #define U585_BOOSTRDY_TIMEOUT 0x40000u
#endif

static uint32_t s_sysclk_hz = 96000000u;

static int u585_try_epod_booster(void)
{
    uint32_t t;

    /* Request VOS=Range 1 AND BOOSTEN=1 atomically (single CR write).
     * Mirrors ST's HAL_PWREx_ControlVoltageScaling MODIFY_REG step. */
    PWR->VOSR = (PWR->VOSR & ~(PWR_VOSR_VOS_Msk | PWR_VOSR_BOOSTEN)) |
                PWR_VOSR_VOS | PWR_VOSR_BOOSTEN;

    /* Wait VOSRDY -- the HAL approach. */
    t = 0;
    while ((PWR->VOSR & PWR_VOSR_VOSRDY) == 0u) {
        if (++t >= U585_BOOSTRDY_TIMEOUT) {
            PWR->VOSR &= ~PWR_VOSR_BOOSTEN;
            return -1;
        }
    }

    /* Wait ACTVOSRDY -- still the HAL approach. */
    t = 0;
    while ((PWR->SVMSR & PWR_SVMSR_ACTVOSRDY) == 0u) {
        if (++t >= U585_BOOSTRDY_TIMEOUT) {
            PWR->VOSR &= ~PWR_VOSR_BOOSTEN;
            return -1;
        }
    }

    /* BOOSTRDY must assert before the 160 MHz PLL config; otherwise
     * the PLL lock would hang indefinitely. On this board (IOT02A)
     * the booster does not latch in our test setup, so we fall back
     * to the 96 MHz path on timeout. */
    t = 0;
    while ((PWR->VOSR & PWR_VOSR_BOOSTRDY) == 0u) {
        if (++t >= U585_BOOSTRDY_TIMEOUT) {
            PWR->VOSR &= ~PWR_VOSR_BOOSTEN;
            return -1;
        }
    }
    return 0;
}

static void clock_init(void)
{
    int boosted;
    uint32_t pll_n;
    uint32_t flash_ws;
    uint32_t spin;

    /* 1) Enable PWR clock so we can write VOSR and CR3 */
    RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
    (void)RCC->AHB3ENR;

    /* 1a) Switch from LDO (reset default) to SMPS supply. ST's
     * SystemClock_Config for B-U585I-IOT02A does this BEFORE setting
     * VOS Range 1 + BOOSTEN; LDO + Range 1 + BOOSTEN can be a path
     * where BOOSTRDY doesn't latch (the booster expects SMPS-driven
     * supply for the 160 MHz tier on this board). */
    PWR->CR3 |= PWR_CR3_REGSEL;
    /* Bounded: on some U585 boards (e.g. B-U585I-IOT02A) the SMPS switch
     * does not latch (CR3.REGSEL reads back 0) and SVMSR.REGS never
     * asserts. The LDO is already active and ready (SVMSR.ACTVOSRDY set),
     * so fall through and run on the LDO rather than spin forever. */
    spin = 0u;
    while ((PWR->SVMSR & PWR_SVMSR_REGS) == 0u) {
        if (++spin > 200000u) {
            break;
        }
    }

    /* 2) Set VOS Range 1 and wait for both regulator-side and
     * active-side ready signals before touching the booster. */
    PWR->VOSR = (PWR->VOSR & ~PWR_VOSR_VOS_Msk) | PWR_VOSR_VOS;
    while ((PWR->VOSR & PWR_VOSR_VOSRDY) == 0u) { }
    while ((PWR->SVMSR & PWR_SVMSR_ACTVOSRDY) == 0u) { }

    /* 3) Try the EPOD booster for 160 MHz. Fall back to 96 MHz if
     * BOOSTRDY doesn't assert within the bounded window. */
    boosted = u585_try_epod_booster();
    if (boosted == 0) {
        pll_n       = 20u;                  /* 16 * 20 / 2 = 160 MHz */
        flash_ws    = FLASH_ACR_LATENCY_4WS;
        s_sysclk_hz = 160000000u;
    }
    else {
        pll_n       = 12u;                  /* 16 * 12 / 2 = 96 MHz */
        flash_ws    = FLASH_ACR_LATENCY_3WS;
        s_sysclk_hz = 96000000u;
    }

    /* 4) FLASH WS at chosen frequency (RM0456 Table 17). */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY_Msk) | flash_ws;
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) != flash_ws) { }

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

    /* 9) PLL1DIVR: N from boost decision (20 if boosted else 12),
     *    R=2. Q+P left default. */
    RCC->PLL1DIVR = ((pll_n - 1u) << RCC_PLL1DIVR_PLL1N_Pos) |
                    ((2u    - 1u) << RCC_PLL1DIVR_PLL1R_Pos);

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

    /* USART1: 8N1, oversample 16. PCLK2 = SYSCLK post-PLL (no APB2
     * prescaler set -> AHB = HCLK = SYSCLK = 96 or 160 MHz). */
    USART1->CR1 = 0;
    USART1->BRR = s_sysclk_hz / 115200u;
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
    clock_init();      /* sets s_sysclk_hz (160 or 96 MHz) */
    uart_init();
    board_common_systick_init(s_sysclk_hz);
}

uint32_t board_sysclk_hz(void)
{
    return s_sysclk_hz;
}


const char *board_name(void)
{
    return "B-U585I-IOT02A";
}

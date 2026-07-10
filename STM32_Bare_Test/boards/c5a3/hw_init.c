/* hw_init.c - STM32C5A3ZG (NUCLEO-C5A3ZG), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-C5A3ZG. Clock + UART config
 * derived from the proven wolfBoot port at hal/stm32c5.c (which has
 * end-to-end UART working on this board):
 *
 *   - HSE 48 MHz (NUCLEO-C5A3ZG soldered crystal)
 *   - PSI: ref source = HSE, ref freq = 48 MHz, output = 144 MHz
 *   - SYSCLK = PSIS = 144 MHz (FLASH 4 WS + prefetch)
 *   - All bus prescalers /1 -> HCLK = PCLK1 = PCLK2 = 144 MHz
 *   - USART2 on PA2 (TX) / PA3 (RX) AF7 -- ST-LINK V3 VCP route on
 *     NUCLEO-C5A3ZG
 *   - BRR = 144000000 / 115200 = 1250 -> 115200 baud 8N1
 *
 * STM32C5A3 has TinyAES + HASH + RNG + SAES + PKA (V2 layout) on
 * Cortex-M33. Same crypto IP set as U585/U545. wolfssl/stm32.h has no
 * WOLFSSL_STM32C5 family arm yet, so user_settings.h pins this board
 * to the SW baseline for now.
 */

#include "stm32c5a3xx.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

/* CMSIS-Core hooks. Vendor startup_stm32c5a3xx.c calls SystemInit() and
 * __PROGRAM_START() during reset. SystemInit() is a no-op (we set up
 * clocks ourselves in clock_init); __PROGRAM_START is the standard
 * newlib symbol that performs C-runtime init then calls main(). */
void SystemInit(void) { /* no-op */ }
__attribute__((weak)) void Default_IRQHandler_Hook(void) {
    while (1) { }
}

/* HSE/PSI ready timeout loop bounds (matches wolfBoot pattern). */
#define HSE_TIMEOUT     0x100000u
#define PSI_TIMEOUT     0x100000u
#define SW_TIMEOUT      0x010000u

#define BOARD_SYSCLK_HZ 144000000u

/* ---- printf retarget over USART2 -------------------------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}


/* ---- Clock init: HSE 48 MHz -> PSI -> PSIS = 144 MHz SYSCLK ----------
 * Mirrors wolfBoot hal/stm32c5.c clock_psi_on(): HSE on, PSIREFSRC=HSE,
 * PSIREF=48 MHz, PSIFREQ=144 MHz, enable PSIS, all bus prescalers /1,
 * flash 4 WS + prefetch + WRHIGHFREQ delay 2, switch SYSCLK to PSIS.
 * On any timeout we leave the chip on its reset clock so boot still
 * proceeds (the rest of the file's BRR/systick numbers will be wrong,
 * but UART/UART-init won't deadlock). CK48 (the RNG kernel clock) is
 * sourced from HSIDIV3 = HSI/3 = 48 MHz so the HW RNG conditions. */
static void clock_init(void)
{
    uint32_t reg;
    volatile uint32_t timeout;

    /* RNG kernel clock = CK48 (RCC_CCIPR2.CK48SEL). Its reset default is NONE,
     * so without this the RNG has no kernel clock at all: conditioning stalls
     * with SR.BUSY stuck and CONDRST never clearing (and CECS=0, since there
     * are no clock edges to flag) -- not a silicon fault, just an unclocked IP.
     * Source CK48 from HSIDIV3 (HSI / 3 = 48 MHz, internal, no crystal
     * dependency). The RNG's own /32 CR.CLKDIV (in the NIST CR value) then
     * samples at the RM-specified rate. HSI is always on; enable its /3 output
     * and point CK48 at it. Validated on silicon: CONDRST clears immediately,
     * DRDY asserts, RNG_DR returns entropy. */
    RCC->CR1 |= RCC_CR1_HSIDIV3ON;
    timeout = HSE_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    reg = RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk;
    reg |= RCC_CCIPR2_CK48SEL_1;   /* CK48 source = HSIDIV3 (HSI/3 = 48 MHz) */
    RCC->CCIPR2 = reg;

    /* 1. Enable HSE (48 MHz on NUCLEO-C5A3ZG soldered crystal). */
    if ((RCC->CR1 & RCC_CR1_HSEON) == 0u) {
        RCC->CR1 |= RCC_CR1_HSEON;
    }
    timeout = HSE_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSERDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    if (timeout == 0u) {
        return;
    }

    /* 2. Configure PSI: ref source = HSE (PSIREFSRC=00), ref = 48 MHz
     * (PSIREF[2:0] = 110b, encoded as PSIREF_2|PSIREF_1), output =
     * 144 MHz (PSIFREQ[1:0] = 01b, encoded as PSIFREQ_0). PSIS, PSIDIV3
     * and PSIK are all off at reset so a plain field write is safe. */
    reg  = RCC->CR2 & ~(RCC_CR2_PSIREFSRC | RCC_CR2_PSIREF |
                        RCC_CR2_PSIFREQ);
    reg |= (0u)                                           /* PSIREFSRC = HSE */
        |  (RCC_CR2_PSIREF_2 | RCC_CR2_PSIREF_1)          /* PSIREF = 48 MHz */
        |  (RCC_CR2_PSIFREQ_0);                           /* PSIFREQ = 144 MHz */
    RCC->CR2 = reg;

    /* 3. Enable PSIS, wait for ready. */
    RCC->CR1 |= RCC_CR1_PSISON;
    timeout = PSI_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_PSISRDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    if (timeout == 0u) {
        return;
    }

    /* 4. All bus prescalers /1. Reset value already 0; explicit for intent. */
    RCC->CFGR2 = 0u;

    /* 5. Flash 4 WS + prefetch BEFORE switching SYSCLK to 144 MHz.
     * LATENCY field carries the raw wait-state count in [3:0]. */
    reg = FLASH->ACR & ~FLASH_ACR_LATENCY;
    reg |= (4u << FLASH_ACR_LATENCY_Pos) | FLASH_ACR_PRFTEN;
    FLASH->ACR = reg;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) !=
           (4u << FLASH_ACR_LATENCY_Pos)) {
        /* spin */
    }

    /* 6. Switch SYSCLK to PSIS (SW=11b = SW_0|SW_1). */
    reg = RCC->CFGR1 & ~RCC_CFGR1_SW;
    RCC->CFGR1 = reg | (RCC_CFGR1_SW_0 | RCC_CFGR1_SW_1);
    timeout = SW_TIMEOUT;
    while (((RCC->CFGR1 & RCC_CFGR1_SWS) !=
            (RCC_CFGR1_SWS_0 | RCC_CFGR1_SWS_1)) && (--timeout != 0u)) {
        /* spin */
    }
    if (timeout == 0u) {
        return;
    }

    /* 7. Programming delay 2 (required at HCLK >= 136 MHz). */
    reg = FLASH->ACR & ~FLASH_ACR_WRHIGHFREQ;
    FLASH->ACR = reg | (2u << FLASH_ACR_WRHIGHFREQ_Pos);

    /* TODO: ICACHE enable (wolfBoot does this). Empirically ICACHE on +
     * 144 MHz shifts the bench reset point earlier (from HMAC-SHA256 to
     * GCM), so the underlying instability is HW-state related, not
     * memory-bandwidth related. Park ICACHE off for now until the
     * 144 MHz HW-AES/HASH timing follow-up is investigated. */
}

/* ---- USART2 init: PA2 (TX) / PA3 (RX), AF7 ---------------------------- */
static void uart_init(void)
{
    /* Enable GPIOA on AHB2 */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

    /* PA2 (TX), PA3 (RX): MODER=AF (10b), AF7 (USART2) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) |
                    (2u << GPIO_MODER_MODE3_Pos);
    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (7u << GPIO_AFRL_AFSEL2_Pos) |
                     (7u << GPIO_AFRL_AFSEL3_Pos);
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD2_Msk | GPIO_PUPDR_PUPD3_Msk);

    /* Enable USART2 clock (APB1L bit USART2EN) */
    RCC->APB1LENR |= RCC_APB1LENR_USART2EN;
    (void)RCC->APB1LENR;

    /* USART2: 8N1, oversample 16. PCLK1 = SYSCLK = 144 MHz after
     * clock_init() (PPRE1 = /1, RCC->CFGR2 = 0). */
    USART2->CR1 = 0;
    USART2->BRR = BOARD_SYSCLK_HZ / 115200u;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART2->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M33F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(BOARD_SYSCLK_HZ);
    /* c5a3 ships no system_stm32c5xx.c, so SystemCoreClock would stay at the
     * weak 0 fallback and the test banner would print "0 Hz / MISMATCH".
     * Set it here (mirrors boards/c562/hw_init.c) to report the real clock. */
    SystemCoreClock = BOARD_SYSCLK_HZ;
}

uint32_t board_sysclk_hz(void)
{
    return BOARD_SYSCLK_HZ;
}


const char *board_name(void)
{
    return "NUCLEO-C5A3ZG";
}

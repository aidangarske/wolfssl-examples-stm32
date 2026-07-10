/* hw_init.c - STM32F767ZI (NUCLEO-F767ZI), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-F767ZI:
 *   - HSI 16 MHz -> PLL -> 216 MHz SYSCLK (Overdrive) + 48 MHz PLL48CLK
 *     for the RNG kernel clock. M7 I-cache + D-cache enabled so the
 *     7-WS flash latency doesn't crater throughput at 216 MHz.
 *   - USART3 on PD8 (TX) / PD9 (RX) AF7, 115200 8N1, ST-LINK VCP
 *     (same pinout as F439, H753, H563 NUCLEO.)
 *
 * STM32F767ZI silicon has RNG only -- no CRYP, no HASH peripheral
 * (only F777xx/F779xx variants ship with HW HASH+CRYP).
 *
 * Overdrive bring-up follows RM0410 Section 5.1.4:
 *   1. HSI on, wait HSIRDY.
 *   2. PWR clock on, set VOS = Range 1.
 *   3. Enable Overdrive (ODEN=1, wait ODRDY).
 *   4. Switch Overdrive (ODSWEN=1, wait ODSWRDY).
 *   5. Flash 7 WS + ART + prefetch.
 *   6. PLL: HSI16 / M=8 = 2 MHz VCO_in, * N=216 = 432 MHz VCO_out,
 *          /P=2 = 216 MHz SYSCLK, /Q=9 = 48 MHz PLL48CLK.
 *   7. AHB/1 = 216, APB1/4 = 54, APB2/2 = 108 (both at max).
 *   8. Switch SYSCLK to PLL.
 */

#include "stm32f7xx.h"
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
static void clock_init(void)
{
    /* HSI 16 MHz -> PLL -> 216 MHz SYSCLK (Overdrive); PLL48CLK = 48 MHz.
     *   PLLM = 8   (VCO_in  = 16/8 = 2 MHz)
     *   PLLN = 216 (VCO_out = 2*216 = 432 MHz)
     *   PLLP = /2  (SYSCLK  = 432/2 = 216 MHz)
     *   PLLQ = /9  (PLL48   = 432/9 = 48 MHz)
     */

    /* 1) HSI on (default) */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }

    /* 2) Enable PWR clock + voltage scale 1 (high performance). */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS_Msk) | PWR_CR1_VOS;  /* VOS1 = 11b */

    /* 3) Enable Overdrive mode (required for SYSCLK > 180 MHz on F76x/77x).
     * The two-step ODEN -> ODSWEN handshake is mandatory per RM0410. */
    PWR->CR1 |= PWR_CR1_ODEN;
    while ((PWR->CSR1 & PWR_CSR1_ODRDY) == 0u) { }
    PWR->CR1 |= PWR_CR1_ODSWEN;
    while ((PWR->CSR1 & PWR_CSR1_ODSWRDY) == 0u) { }

    /* 4) Flash 7 WS at 216 MHz / VOS1 + Overdrive (per RM0410 Table 7) +
     * ART accelerator + prefetch. Without ART the effective code throughput
     * at 7 WS would tank ~7x; with ART + I-cache + D-cache (set in
     * board_init below) the M7 sustains close to its 1 IPC ceiling. */
    FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ARTEN |
                 (7u << FLASH_ACR_LATENCY_Pos);
    while ((FLASH->ACR & FLASH_ACR_LATENCY_Msk) !=
           (7u << FLASH_ACR_LATENCY_Pos)) { }

    /* 5) Make sure SYSCLK is on HSI before reconfiguring PLL */
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0u) { }

    /* 6) PLL off while we configure it */
    RCC->CR &= ~RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) != 0u) { }

    /* 7) PLLCFGR: PLLSRC=HSI (0), M=8, N=216, P=/2 (encoded 0), Q=9 */
    RCC->PLLCFGR = (8u << RCC_PLLCFGR_PLLM_Pos) |
                   (216u << RCC_PLLCFGR_PLLN_Pos) |
                   (0u << RCC_PLLCFGR_PLLP_Pos) |
                   (9u << RCC_PLLCFGR_PLLQ_Pos);

    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0u) { }

    /* 8) Bus prescalers: AHB/1 (=216), APB1/4 (=54 MHz max), APB2/2 (=108
     * MHz max). The APB1 max of 54 MHz at full clock is the binding
     * constraint for the /4 prescaler at SYSCLK=216. */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |
                               RCC_CFGR_PPRE2)) |
                RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 |
                RCC_CFGR_PPRE2_DIV2;

    /* 9) Switch SYSCLK to PLL (SW=10) */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_1;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_1) { }
}

/* ---- USART3 init ------------------------------------------------------ */
static void uart_init(void)
{
    /* Enable GPIOD clock (AHB1 bit 3) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;

    /* PD8 (TX), PD9 (RX): MODER=AF (10b), AF7 (USART3) */
    GPIOD->MODER &= ~(GPIO_MODER_MODER8 | GPIO_MODER_MODER9);
    GPIOD->MODER |= (2u << GPIO_MODER_MODER8_Pos) |
                    (2u << GPIO_MODER_MODER9_Pos);
    GPIOD->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEEDR8_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEEDR9_Pos);
    GPIOD->AFR[1] &= ~((0xFu << GPIO_AFRH_AFRH0_Pos) |
                       (0xFu << GPIO_AFRH_AFRH1_Pos));
    GPIOD->AFR[1] |= (7u << GPIO_AFRH_AFRH0_Pos) |
                     (7u << GPIO_AFRH_AFRH1_Pos);

    /* Enable USART3 clock (APB1ENR bit 18) */
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    /* USART3: 8N1, oversample 16. PCLK1 = 54 MHz post-PLL (216/4). */
    USART3->CR1 = 0;
    USART3->BRR = 54000000u / 115200u;
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    while ((USART3->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        /* spin */
    }
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M7F) */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    /* M7 I-cache + D-cache. The previous 216 MHz attempt skipped these
     * and the M7 ran from flash with 7 WS effective on every fetch,
     * which both tanks throughput AND introduces enough memory-system
     * jitter to wedge ECC SP-math at the M7's compute ceiling. ARM CMSIS
     * SCB_EnableICache / SCB_EnableDCache handle the cache invalidate +
     * enable + barriers. */
    SCB_EnableICache();
    SCB_EnableDCache();

    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(216000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 216000000u;
}


const char *board_name(void)
{
    return "NUCLEO-F767ZI";
}

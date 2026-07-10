/* hw_init_cubemx.c - STM32C5A3 (NUCLEO-C5A3ZG), new-generation HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * C5 ships ST's new-generation HAL ("HAL2"). This brings the board up via the
 * CubeMX-generated mx_system_init() (HAL_Init + 144 MHz clock + NVIC + ICACHE),
 * then adds the RNG kernel clock (CK48, which the generated init does not set)
 * and a register-level USART2 for printf. wolfcrypt drives the crypto IP blocks
 * (SAES/PKA/RNG/CCB) directly via registers -- the new-gen HAL has no classic
 * CRYP/PKA/CCB/RNG driver APIs -- so this build is "HAL board init + register
 * crypto" (WOLFSSL_STM32_BARE, via STM32_HAL_NEWGEN in user_settings.h).
 *
 *   - SYSCLK = 144 MHz (mx_rcc: HSE 48 MHz -> PSI -> PSIS)
 *   - RNG kernel clock CK48 = HSIDIV3 (HSI/3 = 48 MHz)
 *   - USART2 on PA2 (TX) / PA3 (RX) AF7 -- ST-LINK VCP -- BRR = 144e6/115200
 */

#include "stm32c5a3xx.h"
/* New-gen HAL board-init helpers (each pulls stm32_hal.h -> HAL_Init etc.). The
 * generated mx_system.c wrapper is NOT compiled: it defines its own
 * SysTick_Handler (HAL_IncTick), which collides with the harness
 * board_common.c handler (that one already forwards to HAL_IncTick under
 * cubemx). So board_init() replicates the mx_system_init() sequence directly. */
#include "mx_cortex_mpu.h"
#include "mx_cortex_nvic.h"
#include "mx_icache.h"
#include "mx_rcc.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

#define BOARD_SYSCLK_HZ 144000000u
#define C5_TIMEOUT      0x00100000u

/* SystemInit() / SystemCoreClock / AHBPrescTable are provided by the C5A3
 * system_stm32c5xx.c (compiled in for this build); the real clock/HAL setup
 * happens in board_init() via mx_system_init(). The bare build instead defines
 * its own SystemInit no-op (it ships no system file). */
__attribute__((weak)) void Default_IRQHandler_Hook(void) { while (1) { } }

/* ---- printf retarget over USART2 (register level) -------------------- */
void board_putc(int ch)
{
    while ((USART2->ISR & USART_ISR_TXE_TXFNF) == 0) {
        /* wait for TX FIFO space */
    }
    USART2->TDR = (uint32_t)ch & 0xFFu;
}

/* CK48 (RNG kernel clock) = HSIDIV3 = HSI/3 = 48 MHz. The CubeMX-generated
 * mx_rcc does not configure it (the RNG was not selected in that project), but
 * wolfcrypt's register RNG and the SAES self-init need it. Mirrors the CK48
 * block in the bare boards/c5a3/hw_init.c clock_init(). */
static void rng_kernel_clock_init(void)
{
    uint32_t reg;
    volatile uint32_t timeout;

    RCC->CR1 |= RCC_CR1_HSIDIV3ON;
    timeout = C5_TIMEOUT;
    while (((RCC->CR1 & RCC_CR1_HSIDIV3RDY) == 0u) && (--timeout != 0u)) {
        /* spin */
    }
    reg = RCC->CCIPR2 & ~RCC_CCIPR2_CK48SEL_Msk;
    reg |= RCC_CCIPR2_CK48SEL_1;   /* CK48 source = HSIDIV3 (HSI/3 = 48 MHz) */
    RCC->CCIPR2 = reg;
}

/* ---- USART2 init: PA2 (TX) / PA3 (RX), AF7 (register level) ----------
 * PCLK1 = SYSCLK = 144 MHz after mx_system_init (all bus prescalers /1). The
 * new-gen HAL UART driver is intentionally not used -- this register path is
 * identical to the bare board and avoids the new-gen UART config surface. */
static void uart_init(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;

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

    RCC->APB1LENR |= RCC_APB1LENR_USART2EN;
    (void)RCC->APB1LENR;

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

    /* New-gen HAL bring-up -- the mx_system_init() sequence, inlined (see the
     * include note above). HAL_Init() must precede mx_rcc_init(): the new-gen
     * HAL_RCC uses HAL_GetTick() for HSE/clock-switch timeouts, and uwTick is
     * driven by the harness SysTick_Handler (which forwards to HAL_IncTick). */
    (void)mx_cortex_mpu_init();
    (void)HAL_Init();
    (void)mx_cortex_nvic_init();
    (void)mx_icache_init();
    (void)mx_rcc_init();
    (void)mx_rcc_peripherals_clock_config();

    rng_kernel_clock_init();
    uart_init();

    board_common_systick_init(BOARD_SYSCLK_HZ);
    SystemCoreClock = BOARD_SYSCLK_HZ;
}

uint32_t board_sysclk_hz(void)
{
    return BOARD_SYSCLK_HZ;
}

const char *board_name(void)
{
    return "NUCLEO-C5A3ZG (CubeMX/HAL-newgen)";
}

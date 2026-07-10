/* hw_init.c - STM32WL55JC (NUCLEO-WL55JC), bare-metal CMSIS only
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Direct-register board init for NUCLEO-WL55JC:
 *   - HSI 16 MHz default at reset; keep it as SYSCLK (no PLL bring-up)
 *   - LPUART1 on PA2 (TX) / PA3 (RX) AF8, 115200 8N1, ST-LINK V3 VCP
 *     (NUCLEO-WL55JC routes the VCP to LPUART1, not USART1.)
 *
 * WL55JC has TinyAES + RNG + PKA (V1 layout, same as WB55), no HASH
 * peripheral. Dual-core: this app runs on the M4 (CPU1).
 */
#include "stm32wlxx.h"
#include <stdio.h>
#include <stdint.h>
#include "board.h"

void board_putc(int ch)
{
    while ((LPUART1->ISR & USART_ISR_TXE_TXFNF) == 0) { }
    LPUART1->TDR = (uint32_t)ch & 0xFFu;
}


static void clock_init(void) {
    /* Switch SYSCLK from MSI 4 MHz default to HSI 16 MHz so we have
     * enough headroom for ECC SP-math + UART at usable baud. MSI is
     * left at its reset range (4 MHz) and stays running -- it's used
     * as the RNG kernel clock below. */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0u) { }
    /* CFGR.SW: 00=MSI (default), 01=HSI16, 10=HSE, 11=PLL. SW_0=01=HSI. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW_Msk) | RCC_CFGR_SW_0;
    while (((RCC->CFGR & RCC_CFGR_SWS_Msk) >> RCC_CFGR_SWS_Pos) != 1u) { }

    /* Route the RNG kernel clock to MSI (CCIPR.RNGSEL = 11b). The
     * default RNGSEL = 00b (PLL Q) is not running, so the RNG IP
     * stays inert. MSI 4 MHz satisfies the WL RM minimum (kernel
     * clock >= HCLK/16 -> 1 MHz at HCLK 16 MHz). MSI is free-running
     * but can desync under sustained ECDSA-key-gen load -- the BARE
     * wc_GenerateSeed in wolfcrypt/src/random.c now recovers from
     * the resulting SECS/CECS instead of spinning forever. */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_RNGSEL_Msk) |
                 (RCC_CCIPR_RNGSEL_0 | RCC_CCIPR_RNGSEL_1);
}

static void uart_init(void) {
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    (void)RCC->AHB2ENR;
    /* PA2 (TX), PA3 (RX): MODER=AF (10b), AF8 (LPUART1) */
    GPIOA->MODER &= ~(GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE3_Msk);
    GPIOA->MODER |= (2u << GPIO_MODER_MODE2_Pos) | (2u << GPIO_MODER_MODE3_Pos);
    GPIOA->OSPEEDR |= (3u << GPIO_OSPEEDR_OSPEED2_Pos) |
                      (3u << GPIO_OSPEEDR_OSPEED3_Pos);
    GPIOA->AFR[0] &= ~((0xFu << GPIO_AFRL_AFSEL2_Pos) |
                       (0xFu << GPIO_AFRL_AFSEL3_Pos));
    GPIOA->AFR[0] |= (8u << GPIO_AFRL_AFSEL2_Pos) |
                     (8u << GPIO_AFRL_AFSEL3_Pos);
    /* LPUART1 clock on APB1ENR2 */
    RCC->APB1ENR2 |= RCC_APB1ENR2_LPUART1EN;
    (void)RCC->APB1ENR2;
    /* LPUART BRR is 256 * baud_div: at PCLK1 = 16 MHz, divisor = 16e6/115200 = 138.9
     * LPUART formula: LPUART_BRR = (256 * fck) / baud */
    LPUART1->CR1 = 0;
    LPUART1->BRR = (256u * 16000000u) / 115200u;
    LPUART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    while ((LPUART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) { }
}



void board_init(void) {
    SCB->CPACR |= (0xFu << 20);
    __DSB(); __ISB();
    SystemInit();
    clock_init();
    uart_init();
    board_common_systick_init(16000000u);
}
uint32_t board_sysclk_hz(void) { return 16000000u; }
const char *board_name(void) { return "NUCLEO-WL55JC"; }

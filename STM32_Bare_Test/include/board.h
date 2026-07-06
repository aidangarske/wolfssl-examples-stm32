/* board.h - portable board API shim for STM32_Bare_Test
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Each boards/<name>/hw_init.c implements this API. main_test.c and
 * main_bench.c only use these functions, so they stay portable across
 * H5 / F437 / U5 / H7 / etc.
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

/* Forward declarations -- the real types come from the family CMSIS
 * device header that board_common.c already includes. We avoid pulling
 * the family header into board.h so this stays portable. */
struct GPIO_TypeDef;
struct USART_TypeDef;

#ifdef __cplusplus
extern "C" {
#endif

void        board_init(void);
uint32_t    board_sysclk_hz(void);
uint32_t    board_uptime_ms(void);     /* SysTick-driven; resets to 0 at boot */
const char *board_name(void);

/* Shared in boards/common/board_common.c. Each board's hw_init.c
 * implements board_putc and calls board_common_systick_init from
 * board_init. */
void        board_putc(int ch);
void        board_common_systick_init(uint32_t sysclk_hz);

/* Pin-AF and USART core setup helpers shared by boards whose pin and
 * USART register shape matches the modern (G/H/L/U/N) CMSIS layout
 * (MODER / OSPEEDR / AFR[2] for GPIO; CR1 / BRR / ISR / TDR / TEACK /
 * REACK for USART). Caller is responsible for the GPIO and USART
 * kernel-clock enables, which differ by bus / family.
 *
 * board_common_uart_pin_init configures two adjacent GPIO pins as AF.
 *   gpio   -- GPIO_TypeDef* for the bank (e.g. GPIOD)
 *   tx_pin -- 0..15
 *   rx_pin -- 0..15
 *   af     -- 0..15
 *
 * board_common_uart_basic_init brings up the USART with 8N1,
 * oversample 16, TE+RE+UE; waits for TEACK+REACK before returning.
 *   usart    -- USART_TypeDef* (USART1/USART2/USART3/LPUART1/...)
 *   pclk_hz  -- USART kernel clock in Hz (use the *kernel* not bus clock)
 *   baud     -- e.g. 115200
 *
 * For LPUART boards the BRR formula is different (LPUART_BRR =
 * 256*pclk / baud); use the LPUART-specific helper below instead. */
void board_common_uart_pin_init(void *gpio, uint8_t tx_pin,
    uint8_t rx_pin, uint8_t af);
void board_common_uart_basic_init(void *usart, uint32_t pclk_hz,
    uint32_t baud);

#ifdef __cplusplus
}
#endif

#endif /* BOARD_H */

/* hw_init_cubemx.c - STM32F207ZG (NUCLEO-F207ZG), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/f207/hw_init.c. SYSCLK from HSI 16 MHz -> PLL -> 120 MHz.
 *   PLL: M=8 -> 2 MHz, N=120 -> 240 MHz VCO, P=/2 = 120 MHz, Q=/5 = 48 MHz
 *   (VCO must stay within the F2 192-432 MHz range -- RM0033.)
 *   USART3 PD8/PD9 AF7.
 *   F207 has RNG only (no CRYP / HASH IP).
 */

#include "stm32f2xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart3;

void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart3, &b, 1u, HAL_MAX_DELAY);
}

static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* F2 has no voltage-scaling -- skip the SCALE1 setup that newer
     * families need. */
    __HAL_RCC_PWR_CLK_ENABLE();

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;                 /* HSI 16 MHz / 8 = 2 MHz VCO input */
    osc.PLL.PLLN = 120;               /* VCO = 2 * 120 = 240 MHz (F2 192-432) */
    osc.PLL.PLLP = RCC_PLLP_DIV2;     /* SYSCLK = 240/2 = 120 MHz */
    osc.PLL.PLLQ = 5;                 /* PLL48 = 240/5 = 48 MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;       /* 120 MHz HCLK */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;        /* 30 MHz PCLK1 */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;        /* 60 MHz PCLK2 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    s_huart3.Instance = USART3;
    s_huart3.Init.BaudRate = 115200;
    s_huart3.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart3.Init.StopBits = UART_STOPBITS_1;
    s_huart3.Init.Parity = UART_PARITY_NONE;
    s_huart3.Init.Mode = UART_MODE_TX_RX;
    s_huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&s_huart3) != HAL_OK) {
        return -1;
    }
    return 0;
}

void board_init(void)
{
    /* Cortex-M3 -- no FPU. */
    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(120000000u);
}

uint32_t board_sysclk_hz(void) { return 120000000u; }
const char *board_name(void) { return "NUCLEO-F207ZG (CubeMX/HAL)"; }

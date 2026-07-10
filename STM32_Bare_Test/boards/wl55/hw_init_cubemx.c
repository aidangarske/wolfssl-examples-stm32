/* hw_init_cubemx.c - STM32WL55JC (NUCLEO-WL55JC), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/wl55/hw_init.c: HSI 16 MHz SYSCLK (MSI left running
 * at 4 MHz for RNG kernel clock). LPUART1 on PA2/PA3 AF8 -- ST-LINK V3
 * VCP. WL55 M4 has TinyAES + RNG + V1 PKA (no HASH).
 */

#include "stm32wlxx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_hlpuart1;

void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_hlpuart1, &b, 1u, HAL_MAX_DELAY);
}

static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_MSI;
    osc.HSIState   = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.MSIState   = RCC_MSI_ON;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.MSIClockRange = RCC_MSIRANGE_6;     /* 4 MHz */
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_HCLK3 | RCC_CLOCKTYPE_PCLK1 \
                  | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider   = RCC_SYSCLK_DIV1;
    clk.AHBCLK3Divider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider  = RCC_HCLK_DIV1;
    clk.APB2CLKDivider  = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        return -1;
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    periph.RngClockSelection = RCC_RNGCLKSOURCE_MSI;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_LPUART1_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_LPUART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_hlpuart1.Instance = LPUART1;
    s_hlpuart1.Init.BaudRate = 115200;
    s_hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
    s_hlpuart1.Init.StopBits = UART_STOPBITS_1;
    s_hlpuart1.Init.Parity = UART_PARITY_NONE;
    s_hlpuart1.Init.Mode = UART_MODE_TX_RX;
    s_hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_hlpuart1.Init.OverSampling = UART_OVERSAMPLING_16;
    s_hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_hlpuart1) != HAL_OK) {
        return -1;
    }
    return 0;
}

void board_init(void)
{
    /* WL55JC is no-FPU; skip the CPACR / FPCCR setup the other M33/M4F
     * boards do. */
    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(16000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 16000000u;
}

const char *board_name(void)
{
    return "NUCLEO-WL55JC (CubeMX/HAL)";
}

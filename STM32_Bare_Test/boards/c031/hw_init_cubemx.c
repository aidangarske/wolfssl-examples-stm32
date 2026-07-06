/* hw_init_cubemx.c - STM32C031C6 (NUCLEO-C031C6), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * HSI48 / HSIDIV=4 = 12 MHz SYSCLK default. USART2 PA2/PA3 AF1.
 * C0 family has no HW crypto -- wolfcrypt runs in pure software.
 */

#include "stm32c0xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart2;

void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart2, &b, 1u, HAL_MAX_DELAY);
}

static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSIDiv   = RCC_HSI_DIV4;        /* 48 / 4 = 12 MHz */
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    /* C0 has no PLL -- skip PLL.PLLState. */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_huart2.Instance = USART2;
    s_huart2.Init.BaudRate = 115200;
    s_huart2.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart2.Init.StopBits = UART_STOPBITS_1;
    s_huart2.Init.Parity = UART_PARITY_NONE;
    s_huart2.Init.Mode = UART_MODE_TX_RX;
    s_huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    s_huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    s_huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart2) != HAL_OK) {
        return -1;
    }
    return 0;
}

void board_init(void)
{
    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(12000000u);
}

uint32_t board_sysclk_hz(void) { return 12000000u; }
const char *board_name(void) { return "NUCLEO-C031C6 (CubeMX/HAL)"; }

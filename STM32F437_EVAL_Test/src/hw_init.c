/* hw_init.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * HW init for STM32F437IIHx (STM32439I-EVAL / G-EVAL board).
 *
 * Clock: HSI 16 MHz -> PLL (M=8, N=160, P=2) -> 160 MHz SYSCLK
 * UART:  UART4 on PC10 (TX) / PC11 (RX), 115200 8N1
 * Crypto: CRYP, HASH, RNG peripherals (enabled per variant)
 */

#include "stm32f4xx_hal.h"
#include <stdio.h>

UART_HandleTypeDef huart4;

/* UART printf retarget */
#ifdef __GNUC__
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
int _write(int file, char *ptr, int len)
{
    int i;
    (void)file;
    for (i = 0; i < len; i++) {
        __io_putchar(*ptr++);
    }
    return len;
}
#endif

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSI 16 MHz -> PLL -> 160 MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState       = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 160;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        goto fail;
    }

    /* HCLK=160, APB1=40, APB2=80 */
    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) {
        goto fail;
    }
    return;

fail:
    while (1) { __NOP(); }
}

static void UART4_Init(void)
{
    huart4.Instance          = UART4;
    huart4.Init.BaudRate     = 115200;
    huart4.Init.WordLength   = UART_WORDLENGTH_8B;
    huart4.Init.StopBits     = UART_STOPBITS_1;
    huart4.Init.Parity       = UART_PARITY_NONE;
    huart4.Init.Mode         = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart4) != HAL_OK) {
        while (1) { __NOP(); }
    }
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

void hw_init(void)
{
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    UART4_Init();
}

/* HAL MSP callbacks for UART4 GPIO */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio = {0};

    if (huart->Instance == UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        /* UART4: PC10=TX, PC11=RX, AF8 */
        gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF8_UART4;
        HAL_GPIO_Init(GPIOC, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == UART4) {
        __HAL_RCC_UART4_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11);
    }
}

/* HAL MSP callbacks for crypto peripherals (clock enable/disable) */
void HAL_CRYP_MspInit(CRYP_HandleTypeDef *hcryp)
{
    (void)hcryp;
    __HAL_RCC_CRYP_CLK_ENABLE();
}

void HAL_CRYP_MspDeInit(CRYP_HandleTypeDef *hcryp)
{
    (void)hcryp;
    __HAL_RCC_CRYP_CLK_DISABLE();
}

void HAL_HASH_MspInit(HASH_HandleTypeDef *hhash)
{
    (void)hhash;
    __HAL_RCC_HASH_CLK_ENABLE();
}

void HAL_HASH_MspDeInit(HASH_HandleTypeDef *hhash)
{
    (void)hhash;
    __HAL_RCC_HASH_CLK_DISABLE();
}

void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng)
{
    (void)hrng;
    __HAL_RCC_RNG_CLK_ENABLE();
}

void HAL_RNG_MspDeInit(RNG_HandleTypeDef *hrng)
{
    (void)hrng;
    __HAL_RCC_RNG_CLK_DISABLE();
}

void Error_Handler(void)
{
    while (1) { __NOP(); }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    printf("ASSERT: %s:%lu\n", (char *)file, (unsigned long)line);
    while (1) { __NOP(); }
}
#endif

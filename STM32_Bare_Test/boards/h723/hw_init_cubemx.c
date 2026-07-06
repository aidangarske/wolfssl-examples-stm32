/* hw_init_cubemx.c - STM32H723ZG (NUCLEO-H723ZG), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/h723/hw_init.c -- HSI 64 MHz default, no PLL bring-up.
 *   USART3 on PD8 (TX) / PD9 (RX) AF7 -- ST-LINK V3 VCP.
 *   HSI48 on for RNG kernel clock.
 *   H72x sub-family has RNG only -- no CRYP / HASH.
 */

#include "stm32h7xx_hal.h"
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

    /* Supply config first (LDO) to wake VOS-ready */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) { }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState   = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK \
                  | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_HSI;
    clk.SYSCLKDivider   = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider   = RCC_HCLK_DIV1;
    clk.APB3CLKDivider  = RCC_APB3_DIV1;
    clk.APB1CLKDivider  = RCC_APB1_DIV1;
    clk.APB2CLKDivider  = RCC_APB2_DIV1;
    clk.APB4CLKDivider  = RCC_APB4_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) {
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
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
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
    s_huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    s_huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart3) != HAL_OK) {
        return -1;
    }
    return 0;
}

void board_init(void)
{
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(64000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 64000000u;
}

const char *board_name(void)
{
    return "NUCLEO-H723ZG (CubeMX/HAL)";
}

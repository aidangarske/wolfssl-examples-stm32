/* hw_init_cubemx.c - STM32L562E-DK, CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/l562/hw_init.c: HSI 16 MHz SYSCLK.
 *   USART1 on PA9 (TX) / PA10 (RX) AF7 -- ST-LINK V3E VCP.
 *   USART1 kernel clock = PCLK2 (= HSI 16 MHz at this config).
 *   HSI48 enabled for RNG.
 */

#include "stm32l5xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart1;

PKA_HandleTypeDef hpka = { .Instance = PKA };

void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart1, &b, 1u, HAL_MAX_DELAY);
}

static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    RCC_PeriphCLKInitTypeDef periph = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState   = RCC_HSI_ON;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) {
        return -1;
    }

    periph.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    periph.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    s_huart1.Instance = USART1;
    s_huart1.Init.BaudRate = 115200;
    s_huart1.Init.WordLength = UART_WORDLENGTH_8B;
    s_huart1.Init.StopBits = UART_STOPBITS_1;
    s_huart1.Init.Parity = UART_PARITY_NONE;
    s_huart1.Init.Mode = UART_MODE_TX_RX;
    s_huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    s_huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    s_huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart1) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* PKA clock-enable + HAL_PKA_Init. wolfssl's CUBEMX stm32_ecc_sign_hash_ex
 * calls HAL_PKA_ECDSASign(&hpka) assuming the application has run init.
 * Without this, HAL_PKA_ECDSASign returns HAL_ERROR and wolfssl reports
 * WC_HW_E (-248). */
static int pka_init_cubemx(void)
{
    __HAL_RCC_PKA_CLK_ENABLE();
    if (HAL_PKA_Init(&hpka) != HAL_OK) {
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
    (void)pka_init_cubemx();
    board_common_systick_init(16000000u);
}

uint32_t board_sysclk_hz(void) { return 16000000u; }
const char *board_name(void) { return "STM32L562E-DK (CubeMX/HAL)"; }

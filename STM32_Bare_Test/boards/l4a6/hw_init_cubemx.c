/* hw_init_cubemx.c - STM32L4A6ZG (NUCLEO-L4A6ZG), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/l4a6/hw_init.c -- switch from MSI 4 MHz to HSI 16 MHz.
 *   LPUART1 on PG7 (TX) / PG8 (RX) AF8 -- ST-LINK V2 VCP.
 *   PG[7:8] live in the VddIO2 power domain; PWR.CR2.IOSV must be set
 *   to release isolation before driving them.
 *   LPUART1 kernel clock = HSI16.
 *   HSI48 for RNG.
 */

#include "stm32l4xx_hal.h"
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

    /* VddIO2 release for PG[7:8] LPUART pins */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_EnableVddIO2();

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

    /* Route LPUART1 from PCLK1 (which == HSI16 here, since HCLK=APB1=
     * SYSCLK=16 MHz). Sourcing from PCLK1 sidesteps a HAL quirk where
     * LPUART fck lookup via UART_GETCLOCKSOURCE returned the wrong
     * value when LPUART1SEL was set to HSI -- bench output came out
     * garbled. RNG kernel clock stays on HSI48. */
    periph.PeriphClockSelection = RCC_PERIPHCLK_LPUART1 | RCC_PERIPHCLK_RNG;
    periph.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_PCLK1;
    periph.RngClockSelection     = RCC_RNGCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_LPUART1_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF8_LPUART1;
    HAL_GPIO_Init(GPIOG, &gpio);

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
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

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
    return "NUCLEO-L4A6ZG (CubeMX/HAL)";
}

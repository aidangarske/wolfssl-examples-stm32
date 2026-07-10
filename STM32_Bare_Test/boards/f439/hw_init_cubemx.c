/* hw_init_cubemx.c - STM32F439ZI (NUCLEO-F439ZI), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * HAL-based bring-up that mirrors the BARE-metal config in
 * boards/f439/hw_init.c so BUILD=cubemx and BUILD=bare benchmarks are
 * directly comparable. Target SYSCLK is 144 MHz from HSI 16 MHz via PLL:
 *   HSI(16) -> /M=8 -> 2 MHz -> *N=144 -> VCO=288 MHz -> /P=2 = 144 MHz
 *   PLLQ = /6 -> 48 MHz for RNG kernel clock.
 *   HCLK = /1 = 144 MHz; APB1 = /4 = 36 MHz; APB2 = /2 = 72 MHz.
 */

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart3;

/* ---- printf retarget over USART3 -------------------------------------- */
void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart3, &b, 1u, HAL_MAX_DELAY);
}

/* ---- SystemClock_Config ----------------------------------------------- */
static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* HSI + PLL config */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 144;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 6;        /* 48 MHz for RNG kernel clock */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;     /* 144 MHz HCLK */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;      /* 36 MHz APB1 */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;      /* 72 MHz APB2 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- USART3 init (HAL) ------------------------------------------------- */
static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();

    /* PD8 (TX), PD9 (RX): AF7 = USART3. */
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
    if (HAL_UART_Init(&s_huart3) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M4F). HAL_Init does not touch this. */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(144000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 144000000u;
}

const char *board_name(void)
{
    return "NUCLEO-F439ZI (CubeMX/HAL)";
}

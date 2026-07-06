/* hw_init_cubemx.c - STM32F767ZI (NUCLEO-F767ZI), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/f767/hw_init.c. HSI 16 MHz -> PLL -> 216 MHz SYSCLK
 * (Overdrive) + 48 MHz PLL48CLK. USART3 PD8/PD9 AF7. F767 has RNG only
 * (no CRYP/HASH IP).
 *   PLL: M=8 -> 2 MHz, N=216 -> 432 MHz VCO, P=/2 = 216 MHz, Q=/9 = 48 MHz
 *   VOS Scale 1 + Overdrive (R0DEN), FLASH 7 WS.
 */

#include "stm32f7xx_hal.h"
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

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 216;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    /* Enable F7 Overdrive for >180 MHz operation. */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;       /* 216 MHz HCLK */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;        /* 54 MHz PCLK1 */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;        /* 108 MHz PCLK2 */
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_7) != HAL_OK) {
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
    board_common_systick_init(216000000u);
}

uint32_t board_sysclk_hz(void) { return 216000000u; }
const char *board_name(void) { return "NUCLEO-F767ZI (CubeMX/HAL)"; }

/* hw_init_cubemx.c - STM32H7A3ZI-Q (NUCLEO-H7A3ZI-Q), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Mirrors boards/h7a3/hw_init.c:
 *   HSI 64 MHz -> PLL1: M=4 -> 16 MHz ref, *N=35 -> 560 MHz VCO, /P=2
 *     = 280 MHz SYSCLK at VOS Scale 0, FLASH 6 WS
 *   SMPS direct supply (Q-variant silicon)
 *   USART3 PD8/PD9 AF7 @ 115200 from PCLK1
 *
 * H7A3 RNG-only sub-family (no CRYP / HASH IP).
 */

#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart3;
static uint32_t s_sysclk_hz = 64000000u;

void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart3, &b, 1u, HAL_MAX_DELAY);
}

static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* Commit SMPS-direct supply at runtime (matches BARE pattern --
     * doing this via USE_PWR_*_SUPPLY at startup-time hangs the chip
     * with no SWD access on failure). */
    if (HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY) != HAL_OK) {
        /* Fall back to plain HSI 64 MHz if supply commit fails. */
        return -1;
    }

    /* VOS Scale 0 for 280 MHz. */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) { }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState   = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState  = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM      = 4;
    osc.PLL.PLLN      = 35;
    osc.PLL.PLLP      = 2;
    osc.PLL.PLLQ      = 4;
    osc.PLL.PLLR      = 2;
    osc.PLL.PLLRGE    = RCC_PLL1VCIRANGE_3;
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN  = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK \
                  | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource    = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider   = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider   = RCC_HCLK_DIV2;     /* 280/2 = 140 MHz HCLK */
    clk.APB3CLKDivider  = RCC_APB3_DIV2;
    clk.APB1CLKDivider  = RCC_APB1_DIV2;
    clk.APB2CLKDivider  = RCC_APB2_DIV2;
    clk.APB4CLKDivider  = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_6) != HAL_OK) {
        return -1;
    }
    s_sysclk_hz = 280000000u;
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
    board_common_systick_init(s_sysclk_hz);
}

uint32_t board_sysclk_hz(void)
{
    return s_sysclk_hz;
}

const char *board_name(void)
{
    return "NUCLEO-H7A3ZI-Q (CubeMX/HAL)";
}

/* hw_init_cubemx.c - STM32U575ZI (NUCLEO-U575ZI-Q), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * HAL-based bring-up that mirrors the BARE-metal config in
 * boards/u585/hw_init.c so BUILD=cubemx and BUILD=bare benchmarks are
 * directly comparable. Target SYSCLK is 96 MHz from HSI 16 MHz via PLL1:
 *   HSI(16) -> /M=1 -> 16 MHz -> *N=12 -> VCO=192 MHz -> /R=2 = 96 MHz
 * VOS Range 1 alone (no EPOD booster -- EPOD doesn't latch on IOT02A;
 * 96 MHz is the validated ceiling without booster engagement).
 * USART1 on PA9/PA10 AF7 @ 115200, fed from PCLK2 = SYSCLK.
 * HSI48 for RNG kernel clock (default RNGSEL on U5 is HSI48 = 00).
 */

#include "stm32u5xx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart1;

/* U575 has no PKA peripheral; HASH + RNG only. No hpka global needed. */

/* ---- printf retarget over USART1 -------------------------------------- */
void board_putc(int ch)
{
    uint8_t b = (uint8_t)ch;
    (void)HAL_UART_Transmit(&s_huart1, &b, 1u, HAL_MAX_DELAY);
}

/* ---- SystemClock_Config ----------------------------------------------- */
static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* VOS Range 1 -- supports up to 100 MHz without the EPOD booster. */
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
            != HAL_OK) {
        return -1;
    }

    /* HSI16 + HSI48 + PLL1 on; PLL1 source HSI16 -> 96 MHz */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState   = RCC_HSI_ON;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState  = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
    osc.PLL.PLLM      = 1;
    osc.PLL.PLLN      = 12;
    osc.PLL.PLLP      = 2;
    osc.PLL.PLLQ      = 2;
    osc.PLL.PLLR      = 2;
    osc.PLL.PLLRGE    = RCC_PLLVCIRANGE_1;   /* 8-16 MHz input range */
    osc.PLL.PLLFRACN  = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- USART1 init (HAL) ------------------------------------------------- */
static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    /* PA9 (TX), PA10 (RX): AF7 = USART1 */
    gpio.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
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
    s_huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    s_huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart1) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M33F) */
    SCB->CPACR |= (0xFu << 20);
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    board_common_systick_init(96000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 96000000u;
}

const char *board_name(void)
{
    return "NUCLEO-U575ZI-Q (CubeMX/HAL)";
}

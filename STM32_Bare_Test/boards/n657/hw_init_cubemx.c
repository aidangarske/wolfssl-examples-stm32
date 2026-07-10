/* hw_init_cubemx.c - STM32N657X0-Q (NUCLEO-N657X0-Q), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Stay at HSI 64 MHz SYSCLK (matches BARE first-cut config; full PLL
 * bring-up to 600 MHz is a follow-on). USART1 PE5/PE6 AF7.
 *
 * N6 has full HW crypto: TinyAES + HASH + RNG + V2 PKA + SAES + DHUK.
 */

#include "stm32n6xx_hal.h"
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

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSIDiv   = RCC_HSI_DIV1;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL1.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_PCLK4 | RCC_CLOCKTYPE_PCLK5;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_HCLK_DIV1;
    clk.APB1CLKDivider = RCC_APB1_DIV1;
    clk.APB2CLKDivider = RCC_APB2_DIV1;
    clk.APB4CLKDivider = RCC_APB4_DIV1;
    clk.APB5CLKDivider = RCC_APB5_DIV1;
    if (HAL_RCC_ClockConfig(&clk) != HAL_OK) {
        return -1;
    }
    return 0;
}

static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_6;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &gpio);

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
    board_common_systick_init(64000000u);
}

uint32_t board_sysclk_hz(void) { return 64000000u; }
const char *board_name(void) { return "NUCLEO-N657X0-Q (CubeMX/HAL)"; }

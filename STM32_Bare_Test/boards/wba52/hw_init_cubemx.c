/* hw_init_cubemx.c - STM32WBA52CG (NUCLEO-WBA52CG), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * HAL bring-up that mirrors boards/wba52/hw_init.c so BUILD=cubemx and
 * BUILD=bare benchmarks are directly comparable.
 *   HSI16 -> PLL1: M=1, N=25, R=4 -> 400 MHz VCO / 4 = 100 MHz SYSCLK
 *   VOS Range 1, FLASH 3 WS for 100 MHz HCLK
 *   USART1: TX=PB12 / RX=PA8, AF7, 115200 8N1
 *   RNG kernel clock from HSI16 (RCC_RNGCLKSOURCE_HSI)
 */

#include "stm32wbaxx_hal.h"
#include <stdio.h>
#include <stdint.h>

#include "board.h"

static UART_HandleTypeDef s_huart1;

/* Wolfssl CUBEMX PKA path expects the application to supply hpka. */
PKA_HandleTypeDef hpka = { .Instance = PKA };

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
    RCC_PeriphCLKInitTypeDef periph = {0};

    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1)
            != HAL_OK) {
        return -1;
    }

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL1.PLLState  = RCC_PLL_ON;
    osc.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
    osc.PLL1.PLLM      = 1;
    osc.PLL1.PLLN      = 25;
    osc.PLL1.PLLP      = 2;
    osc.PLL1.PLLQ      = 2;
    osc.PLL1.PLLR      = 4;
    osc.PLL1.PLLFractional = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK \
                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_PCLK7 | RCC_CLOCKTYPE_HCLK5;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB7CLKDivider = RCC_HCLK_DIV1;
    clk.AHB5_PLL1_CLKDivider = RCC_SYSCLK_PLL1_DIV4;
    clk.AHB5_HSEHSI_CLKDivider = RCC_SYSCLK_HSEHSI_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_3) != HAL_OK) {
        return -1;
    }

    /* Route RNG kernel clock from HSI16. */
    periph.PeriphClockSelection = RCC_PERIPHCLK_RNG;
    periph.RngClockSelection    = RCC_RNGCLKSOURCE_HSI;
    if (HAL_RCCEx_PeriphCLKConfig(&periph) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- USART1 init (HAL) ------------------------------------------------- */
static int uart_init_cubemx(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    /* PB12 = USART1_TX, PA8 = USART1_RX, both AF7. */
    gpio.Pin       = GPIO_PIN_12;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin       = GPIO_PIN_8;
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
/* PKA clock-enable + HAL_PKA_Init. wolfssl's CUBEMX stm32_ecc_sign_hash_ex
 * calls HAL_PKA_ECDSASign(&hpka) assuming the application has run init
 * (BARE flows handle it via wc_stm32_pka_ensure_init() but that's in
 * the BARE-only section of stm32.c). Without this, HAL_PKA_ECDSASign
 * returns HAL_ERROR and wolfssl reports WC_HW_E (-248). */
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
    /* FPU CP10/CP11 full access (Cortex-M33F) */
    SCB->CPACR |= (0xFu << 20);
    FPU->FPCCR &= ~(FPU_FPCCR_LSPEN_Msk);
    __DSB();
    __ISB();

    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    (void)pka_init_cubemx();
    board_common_systick_init(100000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 100000000u;
}

const char *board_name(void)
{
    return "NUCLEO-WBA52CG (CubeMX/HAL)";
}

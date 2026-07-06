/* hw_init_cubemx.c - STM32H753ZI (NUCLEO-H753ZI), CubeMX/HAL board init
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * HAL-based bring-up that mirrors the BARE-metal config in
 * boards/h7/hw_init.c so BUILD=cubemx and BUILD=bare benchmarks are
 * directly comparable. Target SYSCLK is 480 MHz from HSE 8 MHz BYPASS:
 *   HSE(8) -> /M=1 -> 8 MHz -> *N=120 -> VCO=960 MHz -> /P=2 = 480 MHz
 *   HCLK = SYSCLK/2 = 240 MHz; APBx = HCLK/2 = 120 MHz.
 *
 * NUCLEO-H753ZI HSE is bridged from the ST-LINK MCO via factory-closed
 * solder bridges SB45/SB57, so HSE BYPASS works with no rework.
 */

#include "stm32h7xx_hal.h"
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
/* Mirrors the BARE 480 MHz HSE-driven PLL bring-up. */
static int clock_init_cubemx(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 1) Commit supply (LDO) and request VOS Scale 1 + overdrive. */
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) { }

    /* 2) HSE BYPASS (8 MHz from STLINK MCO), HSI kept ON as transient,
     *    PLL1: source HSE, M=1, N=120, P=2 -> 480 MHz SYSCLK. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE \
                       | RCC_OSCILLATORTYPE_HSI48;
    osc.HSIState = RCC_HSI_DIV1;
    osc.HSEState = RCC_HSE_BYPASS;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 1;
    osc.PLL.PLLN = 120;
    osc.PLL.PLLP = 2;
    osc.PLL.PLLQ = 20;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;   /* 8-16 MHz input */
    osc.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return -1;
    }

    /* 3) Bus prescalers + FLASH latency. With overdrive at 240 MHz HCLK,
     *    H753 needs FLASH_LATENCY_4 (RM0433 Section 4.3.8). */
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK \
                  | RCC_CLOCKTYPE_PCLK1  | RCC_CLOCKTYPE_PCLK2 \
                  | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.SYSCLKDivider = RCC_SYSCLK_DIV1;
    clk.AHBCLKDivider = RCC_HCLK_DIV2;     /* 480/2 = 240 MHz HCLK */
    clk.APB3CLKDivider = RCC_APB3_DIV2;
    clk.APB1CLKDivider = RCC_APB1_DIV2;    /* USART3 kernel clock = 120 MHz */
    clk.APB2CLKDivider = RCC_APB2_DIV2;
    clk.APB4CLKDivider = RCC_APB4_DIV2;
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
    s_huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    s_huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    s_huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&s_huart3) != HAL_OK) {
        return -1;
    }
    return 0;
}

/* ---- Public board API ------------------------------------------------- */
void board_init(void)
{
    /* FPU CP10/CP11 full access (Cortex-M7F). HAL_Init does not touch this. */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();

    SystemInit();
    (void)HAL_Init();
    (void)clock_init_cubemx();
    (void)uart_init_cubemx();
    /* SysTick CLKSOURCE=1 -> CPU clock = 480 MHz */
    board_common_systick_init(480000000u);
}

uint32_t board_sysclk_hz(void)
{
    return 480000000u;
}

const char *board_name(void)
{
    return "NUCLEO-H753ZI (CubeMX/HAL)";
}

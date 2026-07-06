/* stm32u0xx_hal_conf.h - minimal CubeMX HAL config for NUCLEO-U083RC
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Enables the HAL modules wolfcrypt's WOLFSSL_STM32_CUBEMX path needs
 * on U0 silicon (TinyAES + RNG only -- no HASH, no SAES, no PKA).
 */

#ifndef STM32U0xx_HAL_CONF_H
#define STM32U0xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED

#define HAL_RCC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_CRYP_MODULE_ENABLED
/* U0 has no HASH and no PKA -- skip those modules. */
#define HAL_RNG_MODULE_ENABLED

#if !defined(HSE_VALUE)
#define HSE_VALUE                       (16000000U)
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT             (100U)
#endif
#if !defined(MSI_VALUE)
#define MSI_VALUE                       (4000000U)
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE                       (16000000U)
#endif
#if !defined(HSI48_VALUE)
#define HSI48_VALUE                     (48000000U)
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE                       (32000U)
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE                       (32768U)
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT             (5000U)
#endif

/* HAL_RCCEx_GetPeriphCLKFreq() on U3 references these for SAI/audio
 * clock sources we don't wire up. Define them to satisfy the
 * compile-time lookup; runtime never hits these paths. */
#if !defined(EXTERNAL_SAI1_CLOCK_VALUE)
#define EXTERNAL_SAI1_CLOCK_VALUE       (48000U)
#endif
#if !defined(EXTERNAL_SAI2_CLOCK_VALUE)
#define EXTERNAL_SAI2_CLOCK_VALUE       (48000U)
#endif
#if !defined(EXTERNAL_AUDIO_CLOCK_VALUE)
#define EXTERNAL_AUDIO_CLOCK_VALUE      (48000U)
#endif

#define VDD_VALUE                       (3300U)
#define TICK_INT_PRIORITY               (15U)
#define USE_RTOS                        0U
#define PREFETCH_ENABLE                 1U
#define INSTRUCTION_CACHE_ENABLE        1U
#define DATA_CACHE_ENABLE               1U

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32u0xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32u0xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32u0xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32u0xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32u0xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32u0xx_hal_pwr.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32u0xx_hal_uart.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32u0xx_hal_exti.h"
#endif
#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32u0xx_hal_cryp.h"
#endif
#ifdef HAL_RNG_MODULE_ENABLED
#include "stm32u0xx_hal_rng.h"
#endif

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32U0xx_HAL_CONF_H */

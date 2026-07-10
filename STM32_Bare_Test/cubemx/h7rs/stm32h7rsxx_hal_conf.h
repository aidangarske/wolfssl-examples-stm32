/* stm32h7rsxx_hal_conf.h - minimal CubeMX HAL config for NUCLEO-H7S3L8
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * H7S3 silicon has CRYP + HASH + RNG + SAES + V2 PKA. All HAL modules
 * needed for wolfcrypt CUBEMX path enabled.
 */

#ifndef STM32H7RSxx_HAL_CONF_H
#define STM32H7RSxx_HAL_CONF_H

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
#define HAL_HASH_MODULE_ENABLED
#define HAL_RNG_MODULE_ENABLED
#define HAL_PKA_MODULE_ENABLED

#if !defined(HSE_VALUE)
#define HSE_VALUE                       (24000000UL)
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT             (100UL)
#endif
#if !defined(CSI_VALUE)
#define CSI_VALUE                       (4000000UL)
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE                       (64000000UL)
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE                       (32000UL)
#endif
#if !defined(LSE_VALUE)
#define LSE_VALUE                       (32768UL)
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
#define LSE_STARTUP_TIMEOUT             (5000UL)
#endif
#if !defined(EXTERNAL_CLOCK_VALUE)
#define EXTERNAL_CLOCK_VALUE            (12288000UL)
#endif

#define VDD_VALUE                       (3300UL)
#define TICK_INT_PRIORITY               (15UL)
#define USE_RTOS                        0

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32h7rsxx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32h7rsxx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32h7rsxx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32h7rsxx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32h7rsxx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32h7rsxx_hal_pwr.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32h7rsxx_hal_uart.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32h7rsxx_hal_exti.h"
#endif
#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32h7rsxx_hal_cryp.h"
#endif
#ifdef HAL_HASH_MODULE_ENABLED
#include "stm32h7rsxx_hal_hash.h"
#endif
#ifdef HAL_RNG_MODULE_ENABLED
#include "stm32h7rsxx_hal_rng.h"
#endif
#ifdef HAL_PKA_MODULE_ENABLED
#include "stm32h7rsxx_hal_pka.h"
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

#endif /* STM32H7RSxx_HAL_CONF_H */

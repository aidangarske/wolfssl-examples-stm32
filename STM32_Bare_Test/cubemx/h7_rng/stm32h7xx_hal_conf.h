/* stm32h7xx_hal_conf.h - minimal CubeMX HAL config for H7 RNG-only chips
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Variant for STM32H72x / STM32H7A3 etc. -- H7 sub-families that ship
 * RNG only (no CRYP, no HASH IP). HAL_CRYP_MODULE / HAL_HASH_MODULE
 * are NOT enabled here; the regular cubemx/h7 conf is used for full-IP
 * H7 like H753.
 */

#ifndef STM32H7xx_HAL_CONF_H
#define STM32H7xx_HAL_CONF_H

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
#define HAL_RNG_MODULE_ENABLED

#if !defined(HSE_VALUE)
#define HSE_VALUE                       (8000000UL)
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
#if !defined(LSE_VALUE)
#define LSE_VALUE                       (32768UL)
#endif
#if !defined(LSI_VALUE)
#define LSI_VALUE                       (32000UL)
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
#define USE_SD_TRANSCEIVER              0U
#define USE_SPI_CRC                     0U
#define USE_HAL_ETH_REGISTER_CALLBACKS  0U

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32h7xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32h7xx_hal_gpio.h"
#endif
#ifdef HAL_DMA_MODULE_ENABLED
#include "stm32h7xx_hal_dma.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32h7xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32h7xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32h7xx_hal_pwr.h"
#endif
#ifdef HAL_UART_MODULE_ENABLED
#include "stm32h7xx_hal_uart.h"
#endif
#ifdef HAL_EXTI_MODULE_ENABLED
#include "stm32h7xx_hal_exti.h"
#endif
#ifdef HAL_RNG_MODULE_ENABLED
#include "stm32h7xx_hal_rng.h"
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

#endif /* STM32H7xx_HAL_CONF_H */

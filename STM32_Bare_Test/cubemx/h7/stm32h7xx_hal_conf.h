/* stm32h7xx_hal_conf.h - minimal CubeMX HAL config for NUCLEO-H753ZI
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * Slimmed-down derivative of the CubeMX-generated stm32h7xx_hal_conf.h.
 * Enables only the HAL modules wolfcrypt's WOLFSSL_STM32_CUBEMX path
 * needs (CRYP/HASH/RNG) plus the modules required to bring up the
 * board (RCC/PWR/FLASH/GPIO/UART/DMA/CORTEX).
 *
 * NUCLEO-H753ZI HSE is 8 MHz from the ST-LINK MCO via factory-closed
 * solder bridges SB45/SB57 (HSE BYPASS).
 */

#ifndef STM32H7xx_HAL_CONF_H
#define STM32H7xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Module selection -------------------------------------------------- */
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
/* Optional: only if wolfBoot-style timing needed -- currently unused. */
/* #define HAL_TIM_MODULE_ENABLED */

/* ---- Oscillator values ------------------------------------------------- */
/* NUCLEO-H753ZI: HSE from ST-LINK MCO (8 MHz) via factory solder bridges. */
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

/* ---- System config ----------------------------------------------------- */
#define VDD_VALUE                       (3300UL)
#define TICK_INT_PRIORITY               (15UL)
#define USE_RTOS                        0
#define USE_SD_TRANSCEIVER              0U
#define USE_SPI_CRC                     0U

/* H7 dual-core build flag (single-core M7 here) */
#if !defined(DUAL_CORE)
/* DUAL_CORE undefined: H753 is single-core */
#endif

/* ---- Ethernet not used ------------------------------------------------- */
#define USE_HAL_ETH_REGISTER_CALLBACKS  0U

/* ---- HAL assertions disabled (release builds) -------------------------- */
/* #define USE_FULL_ASSERT 1U */

/* ---- Includes for enabled modules -------------------------------------- */
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
#ifdef HAL_CRYP_MODULE_ENABLED
#include "stm32h7xx_hal_cryp.h"
#endif
#ifdef HAL_HASH_MODULE_ENABLED
#include "stm32h7xx_hal_hash.h"
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

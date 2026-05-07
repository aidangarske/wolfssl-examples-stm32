/* stm32_assert.h
 *
 * Copyright (C) 2006-2026 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * STM32 HAL parameter-check macro.
 * Evaluates to a no-op unless USE_FULL_ASSERT is defined; otherwise calls
 * assert_failed() (provided by hw_init.c) which prints location and halts.
 */
#ifndef STM32_ASSERT_H
#define STM32_ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#ifdef USE_FULL_ASSERT
    void assert_failed(uint8_t *file, uint32_t line);
    #define assert_param(expr)                                              \
        ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
#else
    #define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32_ASSERT_H */

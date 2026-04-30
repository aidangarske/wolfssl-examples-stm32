/* stm32u5xx_hal.h
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
 * Shim: wolfSSL's settings.h includes <stm32u5xx_hal.h> when WOLFSSL_STM32U5
 * is defined. On the STM32U3 family we use the U3 HAL. Redirect here.
 * Remove once upstream wolfSSL gains a WOLFSSL_STM32U3 macro.
 */
#ifndef WOLFSSL_STM32U385_HAL_SHIM_H
#define WOLFSSL_STM32U385_HAL_SHIM_H

#include "stm32u3xx_hal.h"

#endif

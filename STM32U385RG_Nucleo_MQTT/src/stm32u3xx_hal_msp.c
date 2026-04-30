/* stm32u3xx_hal_msp.c
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
 * STM32 HAL "MCU Support Package" hooks. HAL_Init() calls HAL_MspInit()
 * before SysTick is set up, and HAL_<peripheral>_Init() calls the
 * per-peripheral MspInit hooks.
 *
 * On the STM32U3, the PWR peripheral clock must be enabled before any
 * HAL_PWREx_* call. SystemClock_Config()'s first call is
 * HAL_PWREx_ConfigSupply(), so HAL_MspInit() is the only place
 * guaranteed to run early enough; without this, the HAL access is
 * silently dropped and SystemClock_Config returns HAL_ERROR.
 *
 * Per-peripheral MspInit hooks (UART, RNG, etc.) are intentionally
 * not implemented — the rest of the demo enables clocks and
 * configures pins directly in hw_init.c and uart_net.c.
 */

#include "main.h"

void HAL_MspInit(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
}

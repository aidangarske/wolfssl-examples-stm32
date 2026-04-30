/* stm32u3xx_it.c
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
 * Cortex-M33 default exception handlers for the STM32U3 demo.
 * Faults sit in a tight loop so a debugger can attach and inspect VTOR /
 * CFSR / HFSR. Production firmware should log and reset.
 *
 * USART1_IRQHandler is provided by uart_net.c (handed to HAL_UART).
 */

#include "main.h"
#include "stm32u3xx_it.h"

void NMI_Handler(void)
{
    while (1) { __NOP(); }
}

void HardFault_Handler(void)
{
    while (1) { __NOP(); }
}

void MemManage_Handler(void)
{
    while (1) { __NOP(); }
}

void BusFault_Handler(void)
{
    while (1) { __NOP(); }
}

void UsageFault_Handler(void)
{
    while (1) { __NOP(); }
}

void SVC_Handler(void)
{
    /* not used */
}

void DebugMon_Handler(void)
{
    /* not used */
}

void PendSV_Handler(void)
{
    /* not used */
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

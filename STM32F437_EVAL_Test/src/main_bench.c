/* main_bench.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * This file is part of wolfssl-examples.
 *
 * wolfssl-examples is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfssl-examples is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */

/*
 * Phase 2 entry — runs wolfCrypt benchmark on bare-metal STM32F437.
 * Output goes to UART4 (PC10/PC11).
 */

#include "stm32f4xx_hal.h"
#include <stdio.h>

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfcrypt/benchmark/benchmark.h"

extern void hw_init(void);

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* current_time stub for benchmark */
double current_time(int reset)
{
    (void)reset;
    return (double)HAL_GetTick() / 1000.0;
}

int main(void)
{
    int ret;
    int cleanup_ret;

    hw_init();

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("\n");
    printf("========================================\n");
    printf("wolfCrypt bench - STM32F437 (CONFIG=%s)\n", BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("SYSCLK: %lu Hz\n", (unsigned long)HAL_RCC_GetSysClockFreq());
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        while (1) { __NOP(); }
    }

    ret = benchmark_test(NULL);

    cleanup_ret = wolfCrypt_Cleanup();
    if (cleanup_ret != 0) {
        printf("wolfCrypt_Cleanup failed: %d\n", cleanup_ret);
        if (ret == 0) {
            ret = cleanup_ret;
        }
    }

    printf("\nBenchmark result: %d (%s)\n", ret, ret == 0 ? "PASS" : "FAIL");
    printf("Benchmark complete\n");

    while (1) { __NOP(); }
    return ret;
}

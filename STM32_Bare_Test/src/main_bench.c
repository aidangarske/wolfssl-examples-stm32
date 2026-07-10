/* main_bench.c
 *
 * Copyright (C) 2026 wolfSSL Inc.
 *
 * wolfCrypt benchmark runner for STM32_Bare_Test.
 */

#include <stdio.h>

#include "board.h"

#include "wolfssl/wolfcrypt/settings.h"
#include "wolfssl/version.h"
#include "wolfssl/wolfcrypt/types.h"
#include "wolfssl/wolfcrypt/wc_port.h"
#include "wolfcrypt/benchmark/benchmark.h"

#ifndef BUILD_CONFIG_NAME
#define BUILD_CONFIG_NAME "unknown"
#endif

/* current_time is referenced by wolfCrypt benchmark */
double current_time(int reset)
{
    (void)reset;
    return (double)board_uptime_ms() / 1000.0;
}

int main(void)
{
    int ret;
    int cleanup_ret;

    board_init();

    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("\n");
    printf("========================================\n");
    printf("wolfCrypt bench - %s (CONFIG=%s)\n", board_name(), BUILD_CONFIG_NAME);
    printf("wolfSSL version: %s\n", LIBWOLFSSL_VERSION_STRING);
    printf("SYSCLK: %lu Hz\n", (unsigned long)board_sysclk_hz());
    printf("========================================\n\n");

    ret = wolfCrypt_Init();
    if (ret != 0) {
        printf("wolfCrypt_Init failed: %d\n", ret);
        for (;;) { }
    }

#if defined(STM32_BARE_STACK_TRACK)
    {
        extern void board_common_stack_track_init(void *frame_ptr);
        /* Paint the carved stack region (linker _Min_Stack_Size) below   */
        /* our current frame; per-bench HWM measurements then start fresh. */
        board_common_stack_track_init(__builtin_frame_address(0));
    }
#endif

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

    for (;;) { }
    return ret;
}

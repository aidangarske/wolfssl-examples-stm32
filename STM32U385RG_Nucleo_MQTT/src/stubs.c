/* stubs.c
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
 * Syscall stubs. rdimon.specs provides _write/_read/etc via semihosting, but
 * _sbrk and time() still need local implementations.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern char end;               /* heap start — linker script */
extern char _estack;           /* top of stack — linker script */
extern char _Min_Stack_Size;   /* reserved stack — linker script */

void *_sbrk(ptrdiff_t incr)
{
    static char *heap_end;
    char *prev;
    char *stack_limit;

    if (heap_end == NULL) {
        heap_end = &end;
    }

    /* Keep heap below the linker-reserved stack region instead of a
     * hard-coded headroom — the reserve lives in stm32u385rg_*.ld and
     * may change. */
    stack_limit = &_estack - (ptrdiff_t)&_Min_Stack_Size;
    if (incr > 0 && heap_end + incr > stack_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    prev = heap_end;
    heap_end += incr;
    return prev;
}

/* Minimal monotonic time — incremented by SysTick if needed. wolfCrypt test
 * doesn't compare against wall clock, so a counter is fine. */
static volatile time_t fake_time_counter = 1704067200;  /* 2024-01-01 */

time_t time(time_t *t)
{
    time_t v = fake_time_counter++;
    if (t) *t = v;
    return v;
}

/* _gettimeofday is provided by rdimon.specs (semihosting syscall). */

/* SYS_WRITE0 (op 0x04): one BKPT, NUL-terminated string. */
static void sh_write0(const char *s)
{
    register uint32_t r0 __asm__("r0") = 0x04;
    register uint32_t r1 __asm__("r1") = (uint32_t)s;
    __asm__ volatile ("bkpt #0xAB" : "+r"(r0) : "r"(r1) : "memory");
}

int _write(int fd, const char *buf, int len)
{
    char tmp[256];
    int sent = 0;
    (void)fd;
    while (sent < len) {
        int chunk = len - sent;
        if (chunk > (int)sizeof(tmp) - 1) chunk = sizeof(tmp) - 1;
        memcpy(tmp, buf + sent, chunk);
        tmp[chunk] = '\0';
        sh_write0(tmp);
        sent += chunk;
    }
    return len;
}

/* Override printf/puts/putchar so the FILE-buffer path is bypassed entirely.
 * newlib-nano's __sputc_r calls __swbuf_r per byte when the stream is
 * unbuffered, and our setvbuf wasn't sticking — so route around stdio. */
int printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;
    buf[n] = '\0';
    sh_write0(buf);
    return n;
}

int puts(const char *s)
{
    sh_write0(s);
    sh_write0("\n");
    return 0;
}

int putchar(int c)
{
    char tmp[2] = { (char)c, '\0' };
    sh_write0(tmp);
    return c;
}

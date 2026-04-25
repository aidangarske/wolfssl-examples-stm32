/* stubs.c
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
 * Syscall stubs for newlib-nano (nosys.specs).
 * Provides _sbrk for malloc and a minimal time() for wolfCrypt test.
 */

#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

extern uint8_t _end;       /* heap start — provided by linker script */
extern uint8_t _estack;    /* top of stack — provided by linker script */
extern uint32_t _Min_Stack_Size; /* stack reservation — linker script */

void *_sbrk(ptrdiff_t incr)
{
    static uint8_t *heap_end;
    uint8_t *prev;
    uint32_t stack_limit;

    if (heap_end == NULL) {
        heap_end = &_end;
    }

    stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;

    if (heap_end + incr > (uint8_t *)stack_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    prev = heap_end;
    heap_end += incr;
    return prev;
}

/* Minimal monotonic time for cert date checking */
static volatile time_t fake_time_counter = 1704067200;  /* 2024-01-01 */

time_t time(time_t *t)
{
    time_t v = fake_time_counter++;
    if (t != NULL) {
        *t = v;
    }
    return v;
}

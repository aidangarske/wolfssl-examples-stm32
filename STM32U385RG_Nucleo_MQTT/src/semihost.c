/* semihost.c
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
 * Minimal semihosting bring-up.
 *
 * Links against newlib's rdimon specs (--specs=rdimon.specs -lrdimon), which
 * provides _write/_read/_close/etc. routed through BKPT 0xAB to the attached
 * debugger (OpenOCD or gdb with `monitor arm semihosting enable`).
 *
 * Only thing we need to do here is prime newlib via initialise_monitor_handles()
 * so that stdout/stderr dispatch goes through the semihosting SYS_WRITEC path.
 */

#include <stdio.h>

extern void initialise_monitor_handles(void);

void semihost_init(void)
{
    initialise_monitor_handles();
    /* rdimon's init forces stdout to _IONBF; switch to line-buffering so
     * printf batches per-line into one SYS_WRITE0 BKPT (see _write in
     * stubs.c) instead of one BKPT per byte. */
    static char stdout_buf[256];
    setvbuf(stdout, stdout_buf, _IOLBF, sizeof(stdout_buf));
}

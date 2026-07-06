/* stubs.c - newlib syscall stubs for bare-metal STM32
 *
 * Copyright (C) 2026 wolfSSL Inc.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>

extern uint8_t _end;
extern uint32_t _heap_limit;

static char *heap_end;

void *_sbrk(ptrdiff_t incr)
{
    char *prev;
    if (heap_end == 0) {
        heap_end = (char *)&_end;
    }
    prev = heap_end;
    if ((heap_end + incr) >= (char *)&_heap_limit) {
        errno = ENOMEM;
        return (void *)-1;
    }
    heap_end += incr;
    return prev;
}

int _close(int file)        { (void)file; return -1; }
int _isatty(int file)       { (void)file; return 1; }
int _lseek(int file, int p, int d) { (void)file; (void)p; (void)d; return 0; }
int _read(int file, char *p, int len) { (void)file; (void)p; (void)len; return 0; }
int _kill(int pid, int sig) { (void)pid; (void)sig; errno = EINVAL; return -1; }
int _getpid(void)           { return 1; }
void _exit(int status)      { (void)status; for (;;) { } }

int _fstat(int file, struct stat *st)
{
    (void)file;
    if (st == 0) { errno = EINVAL; return -1; }
    st->st_mode = S_IFCHR;
    return 0;
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

#include "barr_debug.h"
#include "barr_env.h"
#include "barr_io.h"

#include <execinfo.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#if defined(BARR_DEBUG)
static atomic_flag barr_dbg_log_lock = ATOMIC_FLAG_INIT;
#endif

struct timespec barr_dbg_ts = {.tv_sec = 0, .tv_nsec = 100 * 100};

void BARR_dumb_backtrace(void)
{
#ifndef BARR_DEBUG
    return;
#endif
    void *buf[BARR_BUF_SIZE_32];
    barr_i32 nptrs = backtrace(buf, 32);

    BARR_warnlog("---- Backtrace (%d frames) ----", nptrs);
    char **symbols = backtrace_symbols(buf, nptrs);
    if (!symbols)
    {
        BARR_warnlog("Failed to resolve symbols");
        return;
    }

    for (barr_i32 i = 0; i < nptrs; i++)
    {
        BARR_warnlog(" [%02d] %s", i, symbols[i]);
    }

    free(symbols);
}

void BARR_dbglog(const char *format, ...)
{
#if defined(BARR_DEBUG)
    while (atomic_flag_test_and_set(&barr_dbg_log_lock))
    {
        nanosleep(&barr_dbg_ts, NULL);
    }
    barr_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        printf("\033[36;1m[barr_debug]: ");
    }
    else
    {
        printf("[barr_debug]: ");
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (is_tty)
    {
        printf("\033[0m\n");
    }
    else
    {
        printf("\n");
    }
    atomic_flag_clear(&barr_dbg_log_lock);
#else
    BARR_VOID(format);
#endif
}

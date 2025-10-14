#include "atl_debug.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"

#include <execinfo.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#if defined(ATL_DEBUG)
static atomic_flag atl_dbg_log_lock = ATOMIC_FLAG_INIT;
#endif

struct timespec atl_dbg_ts = {.tv_sec = 0, .tv_nsec = 100 * 100};

void ATL_dumb_backtrace(void)
{
#ifndef ATL_DEBUG
    return;
#endif
    void *buf[ATL_BUF_SIZE_32];
    atl_i32 nptrs = backtrace(buf, 32);

    ATL_warnlog("---- Backtrace (%d frames) ----", nptrs);
    char **symbols = backtrace_symbols(buf, nptrs);
    if (!symbols)
    {
        ATL_warnlog("Failed to resolve symbols");
        return;
    }

    for (atl_i32 i = 0; i < nptrs; i++)
    {
        ATL_warnlog(" [%02d] %s", i, symbols[i]);
    }

    free(symbols);
}

void ATL_dbglog(const char *format, ...)
{
#if defined(ATL_DEBUG)
    while (atomic_flag_test_and_set(&atl_dbg_log_lock))
    {
        nanosleep(&atl_dbg_ts, NULL);
    }
    atl_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        printf("\033[36;1m[atl_debug]: ");
    }
    else
    {
        printf("[atl_debug]: ");
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
    atomic_flag_clear(&atl_dbg_log_lock);
#else
    ATL_VOID(format);
#endif
}

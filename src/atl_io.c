#include "atl_io.h"

#include <errno.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static atomic_flag atl_io_file_write_lock = ATOMIC_FLAG_INIT;
static atomic_flag atl_io_log_lock = ATOMIC_FLAG_INIT;

struct timespec atl_ts = {.tv_sec = 0, .tv_nsec = 100 * 100};

void ATL_printf(const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_log_lock))
    {
        nanosleep(&atl_ts, NULL);
    }
    atl_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        printf("\033[34;1m");
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    if (is_tty)
    {
        printf("\033[0m");
    }
    fflush(stdout);
    atomic_flag_clear(&atl_io_log_lock);
}

void ATL_log(const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_log_lock))
    {
        nanosleep(&atl_ts, NULL);
    }
    atl_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        printf("\033[32;1m[atl_log]: ");
    }
    else
    {
        printf("[atl_log]: ");
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
    atomic_flag_clear(&atl_io_log_lock);
}

void ATL_warnlog(const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_log_lock))
    {
        nanosleep(&atl_ts, NULL);
    }
    atl_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        fprintf(stderr, "\033[38;2;255;165;0m[atl_warning]: ");
    }
    else
    {
        fprintf(stderr, "[atl_warning]: ");
    }

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (errno)
    {
        fprintf(stderr, " | \033[38;2;255;165;0m[stderr]: %s: %d", strerror(errno), errno);
        errno = 0;
    }
    if (is_tty)
    {
        fprintf(stderr, "\033[0m\n");
    }
    else
    {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    atomic_flag_clear(&atl_io_log_lock);
}

void ATL_errlog(const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_log_lock))
    {
        nanosleep(&atl_ts, NULL);
    }
    atl_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        fprintf(stderr, "\033[31;1m[atl_error]: ");
    }
    else
    {
        fprintf(stderr, "[atl_error]: ");
    }

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (errno)
    {
        fprintf(stderr, " | \033[31;1m[stderr]: %s: %d", strerror(errno), errno);
        errno = 0;
    }
    if (is_tty)
    {
        fprintf(stderr, "\033[0m\n");
    }
    else
    {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    atomic_flag_clear(&atl_io_log_lock);
}

void ATL_dbglog(const char *format, ...)
{
#if defined(ATL_DEBUG)
    while (atomic_flag_test_and_set(&atl_io_log_lock))
    {
        nanosleep(&atl_ts, NULL);
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
    atomic_flag_clear(&atl_io_log_lock);
#else
    ATL_VOID(format);
#endif
}

// file io

atl_i32 ATL_file_write(const char *filename, const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_file_write_lock))
    {
        nanosleep(&atl_ts, NULL);
    }

    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        ATL_errlog("Failed to open: %s", filename);
        return -1;
    }

    va_list args;
    va_start(args, format);
    atl_i32 result = vfprintf(fp, format, args);
    va_end(args);

    fclose(fp);

    atomic_flag_clear(&atl_io_file_write_lock);
    return result >= 0;
}

atl_i32 ATL_file_append(const char *filename, const char *format, ...)
{
    while (atomic_flag_test_and_set(&atl_io_file_write_lock))
    {
        nanosleep(&atl_ts, NULL);
    }
    FILE *fp = fopen(filename, "a");
    if (!fp)
    {
        // errlog
        return -1;
    }

    va_list args;
    va_start(args, format);
    atl_i32 result = vfprintf(fp, format, args);
    va_end(args);

    fclose(fp);
    atomic_flag_clear(&atl_io_file_write_lock);
    return result >= 0;
}

//----------------------------------------------------------------------------------------------------

atl_i32 ATL_file_copy(const char *src, const char *dst)
{
    FILE *in = fopen(src, "r");
    if (!in)
    {
        ATL_errlog("Failed to open file for read");
        return 1;
    }

    FILE *out = fopen(dst, "w");
    if (!out)
    {
        ATL_errlog("Failed to open file to write");
        fclose(in);
        return 1;
    }

    char buf[ATL_BUF_SIZE_4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n)
        {
            ATL_errlog("Failed to write to buf");
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

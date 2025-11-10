#include "barr_io.h"
#include "barr_debug.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static atomic_flag barr_io_file_write_lock = ATOMIC_FLAG_INIT;
static atomic_flag barr_io_log_lock = ATOMIC_FLAG_INIT;

struct timespec barr_ts = {.tv_sec = 0, .tv_nsec = 100 * 100};

void BARR_printf(const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_log_lock))
    {
        nanosleep(&barr_ts, NULL);
    }
    barr_i32 is_tty = isatty(fileno(stdout));
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
    atomic_flag_clear(&barr_io_log_lock);
}

void BARR_log(const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_log_lock))
    {
        nanosleep(&barr_ts, NULL);
    }
    barr_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        printf("\033[32;1m[barr_log]: ");
    }
    else
    {
        printf("[barr_log]: ");
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
    atomic_flag_clear(&barr_io_log_lock);
}

void BARR_warnlog(const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_log_lock))
    {
        nanosleep(&barr_ts, NULL);
    }
    barr_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        fprintf(stderr, "\033[38;2;255;165;0m[barr_warning]: ");
    }
    else
    {
        fprintf(stderr, "[barr_warning]: ");
    }

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    if (is_tty)
    {
        fprintf(stderr, "\033[0m\n");
    }
    else
    {
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    atomic_flag_clear(&barr_io_log_lock);
}

void BARR_errlog(const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_log_lock))
    {
        nanosleep(&barr_ts, NULL);
    }
    barr_i32 is_tty = isatty(fileno(stdout));
    if (is_tty)
    {
        fprintf(stderr, "\033[31;1m[barr_error]: ");
    }
    else
    {
        fprintf(stderr, "[barr_error]: ");
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
    atomic_flag_clear(&barr_io_log_lock);
}

// file io

barr_i32 BARR_file_write(const char *filename, const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_file_write_lock))
    {
        nanosleep(&barr_ts, NULL);
    }

    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        BARR_errlog("Failed to open: %s", filename);
        return -1;
    }

    va_list args;
    va_start(args, format);
    barr_i32 result = vfprintf(fp, format, args);
    va_end(args);

    fclose(fp);

    atomic_flag_clear(&barr_io_file_write_lock);
    return result >= 0;
}

barr_i32 BARR_file_append(const char *filename, const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_file_write_lock))
    {
        nanosleep(&barr_ts, NULL);
    }
    FILE *fp = fopen(filename, "a");
    if (!fp)
    {
        // errlog
        return -1;
    }

    va_list args;
    va_start(args, format);
    barr_i32 result = vfprintf(fp, format, args);
    va_end(args);

    fclose(fp);
    atomic_flag_clear(&barr_io_file_write_lock);
    return result >= 0;
}

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_file_copy(const char *src, const char *dst)
{
    FILE *in = fopen(src, "r");
    if (!in)
    {
        BARR_errlog("Failed to open file for read");
        return 1;
    }

    FILE *out = fopen(dst, "w");
    if (!out)
    {
        BARR_errlog("Failed to open file to write");
        fclose(in);
        return 1;
    }

    char buf[BARR_BUF_SIZE_4096];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n)
        {
            BARR_errlog("Failed to write to buf");
            fclose(in);
            fclose(out);
            return 1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

//-----------------------------------------------------------------------

static barr_i32 barr_copy_file_contents(barr_i32 infd, barr_i32 outfd)
{
    ssize_t nr;
    ssize_t nw;
    char buf[BARR_FS_COPY_BUF];

    while (INFINITY)
    {
        nr = read(infd, buf, sizeof(buf));
        if (nr < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }

        if (nr == 0)
        {
            break;
        }

        char *p = buf;
        ssize_t remaining = nr;

        while (remaining > 0)
        {
            nw = write(outfd, p, remaining);
            if (nw < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return -1;
            }

            remaining -= nw;
            p += nw;
        }
    }

    return 0;
}

barr_i32 BARR_fs_copy(const char *src, const char *dst)
{
    if (src == NULL || dst == NULL)
    {
        return 1;
    }

    struct barr_stat st;

    barr_i32 infd = -1;
    barr_i32 outfd = -1;
    barr_i32 ret = -1;

    char dst_dir[BARR_PATH_MAX];
    char *slash;

    if (lstat(src, &st) != 0)
    {
        return -1;
    }

    if (S_ISLNK(st.st_mode))
    {
        ssize_t len;
        char link_target[BARR_PATH_MAX];

        len = readlink(src, link_target, sizeof(link_target) - 1);
        if (len < 0)
        {
            return -1;
        }

        link_target[len] = '\0';

        // remove dst if exist
        unlink(dst);

        if (symlink(link_target, dst) != 0)
        {
            return -1;
        }

        return 0;
    }

    if (!S_ISREG(st.st_mode))
    {
        return -1;
    }

    infd = open(src, O_RDONLY);
    if (infd < 0)
    {
        return -1;
    }

    strncpy(dst_dir, dst, sizeof(dst_dir) - 1);
    dst_dir[sizeof(dst_dir) - 1] = '\0';

    slash = strrchr(dst_dir, '/');
    if (slash != NULL)
    {
        *slash = '\0';
        if (BARR_mkdir_p(dst_dir) != 0)
        {
            goto cleanup;
        }
    }

    outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 07777);
    if (outfd < 0)
    {
        goto cleanup;
    }

    if (barr_copy_file_contents(infd, outfd) != 0)
    {
        goto cleanup;
    }

    // flush and sync
    if (fsync(outfd) != 0)
    {
        goto cleanup;
    }

    if (fchmod(outfd, st.st_mode & 07777) != 0)
    {
        goto cleanup;
    }

#if defined(HAVE_UTIMENSAT) || _POSIX_C_SOURCE >= 200809L
    struct timespec times[2];
#ifdef BARR_OS_MACOS
    struct timeval tv[2];
    tv[0].tv_sec = st.st_atime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = st.st_mtime;
    tv[1].tv_usec = 0;
    utimes(dst, tv);
#else
    times[0] = st.st_atim;
    times[1] = st.st_mtim;
    if (utimensat(AT_FDCWD, dst, times, 0) != 0)
    {
        /* non-fatal on some filesystems; continue */
    }
#endif
#else
    struct utimbuf tb;
    tb.actime = st.st_atime;
    tb.modtime = st.st_mtime;
    utime(dst, &tb);
#endif

    ret = 0;

cleanup:

    if (infd > 0)
    {
        close(infd);
    }

    if (outfd > 0)
    {
        close(outfd);
    }

    return ret;
}

barr_i32 BARR_fs_copy_tree(const char *src_dir, const char *dst_dir)
{
    if (src_dir == NULL || dst_dir == NULL)
    {
        return 1;
    }

    DIR *d = NULL;
    struct dirent *ent = NULL;
    struct barr_stat st;

    char src_path[BARR_PATH_MAX];
    char dst_path[BARR_PATH_MAX];

    if (lstat(src_dir, &st) != 0)
    {
        return -1;
    }

    if (!S_ISDIR(st.st_mode))
    {
        errno = ENOTDIR;
        return -1;
    }

    if (BARR_mkdir_p(dst_dir) != 0)
    {
        return -1;
    }

    d = opendir(src_dir);
    if (d == NULL)
    {
        return -1;
    }

    while ((ent = readdir(d)) != NULL)
    {
        if (BARR_strmatch(ent->d_name, ".") || BARR_strmatch(ent->d_name, ".."))
        {
            continue;
        }

        if (snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, ent->d_name) >= (barr_i32) sizeof(src_path))
        {
            closedir(d);
            return -1;
        }

        if (snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, ent->d_name) >= (barr_i32) sizeof(dst_path))
        {
            closedir(d);
            return -1;
        }

        if (lstat(src_path, &st) != 0)
        {
            closedir(d);
            return -1;
        }

        if (S_ISDIR(st.st_mode))
        {
            if (BARR_fs_copy_tree(src_path, dst_path) != 0)
            {
                closedir(d);
                return -1;
            }
        }
        else if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
        {
            if (BARR_fs_copy(src_path, dst_path) != 0)
            {
                closedir(d);
                return -1;
            }
        }
        else
        {
            continue;
        }
    }

    closedir(d);
    return 0;
}

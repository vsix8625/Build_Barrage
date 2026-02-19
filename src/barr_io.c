#define _POSIX_C_SOURCE 200809L /* for utimensat */

#include "barr_gc.h"

#include "barr_config.h"
#include "barr_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/sendfile.h>
#include <time.h>

static atomic_flag barr_io_file_write_lock = ATOMIC_FLAG_INIT;
static atomic_flag barr_io_log_lock        = ATOMIC_FLAG_INIT;

struct timespec barr_ts = {.tv_sec = 0, .tv_nsec = 100 * 100};

void BARR_printf(const char *format, ...)
{
    while (atomic_flag_test_and_set(&barr_io_log_lock))
    {
        nanosleep(&barr_ts, NULL);
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

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
        printf("\033[32;1m[barr_log]:\033[0m ");
    }
    else
    {
        printf("[barr_log]: ");
    }

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
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
        fprintf(stderr, "\033[38;2;255;165;0m[barr_warning]:\033[0m ");
    }
    else
    {
        fprintf(stderr, "[barr_warning]: ");
    }

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
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
        fprintf(stderr, "\033[31;1m[barr_error]:\033[0m ");
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

    fprintf(stderr, "\n");
    fflush(stderr);
    atomic_flag_clear(&barr_io_log_lock);
}

char *BARR_file_read(const char *filepath)

{
    if (filepath == NULL)
    {
        BARR_errlog("%s(): NULL path", __func__);
        return NULL;
    }

    FILE *f = fopen(filepath, "r");

    if (f == NULL)
    {
        BARR_errlog("%s(): cannot open file '%s'", __func__, filepath);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        BARR_errlog("%s(): fseek failed", __func__);
        fclose(f);
        return NULL;
    }

    barr_i64 size = ftell(f);

    if (size < 0)
    {
        BARR_errlog("%s(): ftell failed", __func__);
        fclose(f);
        return NULL;
    }
    rewind(f);

    char *buf = BARR_gc_alloc((size_t) size + 1);

    if (buf == NULL)
    {
        BARR_errlog("%s(): BARR_gc_alloc failed", __func__);
        fclose(f);
        return NULL;
    }

    size_t read_size = fread(buf, 1, (size_t) size, f);
    buf[read_size]   = '\0';

    fclose(f);
    return buf;
}

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
    return result >= 0 ? 0 : -1;
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
    return result >= 0 ? 0 : -1;
}

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_file_copy(const char *src, const char *dst)
{
    struct stat st;

    if (stat(src, &st) != 0)
    {
        BARR_errlog("Failed to stat source file");
        return 1;
    }

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

    char   buf[BARR_BUF_SIZE_4096];
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

    if (chmod(dst, st.st_mode) != 0)
    {
        BARR_errlog("Failed to chmod destination");
        return 1;
    }

    return 0;
}

//-----------------------------------------------------------------------

// TODO: optimize fs_copy
#define BARR_COPY_BUF_SIZE (256 * 1024)

static barr_i32 barr_copy_file_contents(barr_i32 infd, barr_i32 outfd)
{
    char buf[BARR_COPY_BUF_SIZE];

    ssize_t nr;

    while ((nr = read(infd, buf, sizeof(buf))) > 0)
    {
        char   *p         = buf;
        ssize_t remaining = nr;

        while (remaining > 0)
        {
            ssize_t nw = write(outfd, p, remaining);
            if (nw < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return -1;
            }

            remaining -= nw;
            p         += nw;
        }
    }

    return (nr < 0 && errno != EINTR) ? -1 : 0;
}

static barr_i32 barr_ensure_parent_dir(const char *path)
{
    char        tmp[BARR_PATH_MAX];
    const char *slash = strrchr(path, BARR_PATH_SEPARATOR_CHAR);

    if (slash == NULL || slash == path)
    {
        return 0;
    }

    size_t len = (size_t) (slash - path);
    if (len >= sizeof(tmp))
    {
        return -1;
    }

    memcpy(tmp, path, len);
    tmp[len] = '\0';
    return BARR_mkdir_p(tmp);
}

barr_i32 BARR_fs_copy(const char *src, const char *dst)
{
    struct barr_stat st;
    barr_i32         infd   = -1;
    barr_i32         outfd  = -1;
    barr_i32         ret    = -1;
    int              copied = 0;

    if (src == NULL || dst == NULL)
    {
        return 1;
    }
    if (lstat(src, &st) != 0)
    {
        return -1;
    }

    /* --- symlinks --- */
    if (S_ISLNK(st.st_mode))
    {
        char    link_target[BARR_PATH_MAX];
        ssize_t len = readlink(src, link_target, sizeof(link_target) - 1);
        if (len < 0)
        {
            return -1;
        }
        link_target[len] = '\0';
        (void) unlink(dst);
        return symlink(link_target, dst) == 0 ? 0 : -1;
    }

    if (!S_ISREG(st.st_mode))
    {
        return -1;
    }

    infd = open(src, O_RDONLY | O_CLOEXEC);
    if (infd < 0)
    {
        return -1;
    }

    if (barr_ensure_parent_dir(dst) != 0)
    {
        goto cleanup;
    }

    outfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, st.st_mode & 07777);
    if (outfd < 0)
    {
        goto cleanup;
    }

#if defined(BARR_OS_LINUX) && defined(BARR_HAVE_COPY_FILE_RANGE)
    {
        off64_t off_in = 0, off_out = 0;
        while (off_in < st.st_size)
        {
            ssize_t n =
                copy_file_range(infd, &off_in, outfd, &off_out, (size_t) (st.st_size - off_in), 0);
            if (n > 0)
            {
                continue;
            }
            if (n == 0)
            {
                copied = 1;
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EXDEV || errno == EINVAL || errno == ENOSYS)
            {
                break;
            }
            goto cleanup;
        }
        if (off_in >= st.st_size)
        {
            copied = 1;
        }
    }
#endif

#if defined(BARR_OS_LINUX) && defined(BARR_HAVE_SENDFILE)
    if (!copied)
    {
        off_t off = 0;
        while (off < st.st_size)
        {
            ssize_t n = sendfile(outfd, infd, &off, (size_t) (st.st_size - off));
            if (n > 0)
            {
                continue;
            }
            if (n == 0)
            {
                copied = 1;
                break;
            }
            if (errno == EINTR)
            {
                continue;
            }
            if (errno == EINVAL || errno == ENOSYS)
            {
                break;
            }
            goto cleanup;
        }
        if (off >= st.st_size)
        {
            copied = 1;
        }
    }
#endif

    if (!copied)
    {
        if (barr_copy_file_contents(infd, outfd) != 0)
        {
            goto cleanup;
        }
    }

    /* --- ALWAYS: metadata ---
     * who needs safety if they can fly
     *if (fsync(outfd) != 0)
     *{
     *    goto cleanup;
     *}
     */

    if (fchmod(outfd, st.st_mode & 07777) != 0)
    {
        goto cleanup;
    }

#if defined(BARR_HAVE_UTIMENSAT) || _POSIX_C_SOURCE >= 200809L
    {
        struct timespec ts[2] = {st.st_atim, st.st_mtim};
#ifdef BARR_OS_MACOS
        struct timeval tv[2];
        TIMESPEC_TO_TIMEVAL(&tv[0], &ts[0]);
        TIMESPEC_TO_TIMEVAL(&tv[1], &ts[1]);
        (void) utimes(dst, tv);
#else
        (void) utimensat(AT_FDCWD, dst, ts, 0);
#endif
    }
#else
    {
        struct utimbuf tb = {.actime = st.st_atime, .modtime = st.st_mtime};
        (void) utime(dst, &tb);
    }
#endif

    ret = 0;

cleanup:

    if (infd >= 0)
    {
        close(infd);
    }

    if (outfd >= 0)
    {
        close(outfd);
    }

    return ret;
}

//--------------------------------------------------------------------------

static barr_i32
barr_copy_tree_entry(const char *src_base, const char *dst_base, const struct dirent *ent)
{
    char             src_path[BARR_PATH_MAX];
    char             dst_path[BARR_PATH_MAX];
    struct barr_stat st;

    barr_i32 n1 = snprintf(src_path, sizeof(src_path), "%s/%s", src_base, ent->d_name);
    barr_i32 n2 = snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_base, ent->d_name);

    if (n1 < 0 || n1 >= (barr_i32) sizeof(src_path) || n2 < 0 || n2 >= (barr_i32) sizeof(dst_path))
    {
        return -1;
    }

    if (lstat(src_path, &st) != 0)
    {
        return -1;
    }

    if (S_ISDIR(st.st_mode))
    {
        return BARR_fs_copy_tree(src_path, dst_path);
    }

    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))
    {
        return BARR_fs_copy(src_path, dst_path);
    }

    return 0;
}

barr_i32 BARR_fs_copy_tree(const char *src_dir, const char *dst_dir)
{
    DIR             *d   = NULL;
    struct dirent   *ent = NULL;
    struct barr_stat st;

    if (src_dir == NULL || dst_dir == NULL)
    {
        return 1;
    }

    if (lstat(src_dir, &st) != 0 || !S_ISDIR(st.st_mode))
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

        if (barr_copy_tree_entry(src_dir, dst_dir, ent) != 0)
        {
            closedir(d);
            return -1;
        }
    }

    closedir(d);
    return 0;
}

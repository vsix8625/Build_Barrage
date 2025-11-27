#define _POSIX_C_SOURCE 200809L /* for utimensat */

#include <stdlib.h>

#include <fcntl.h>
#include <linux/version.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__GNUC__) || defined(__clang__)
#define CAN_CALL(func) __builtin_choose_expr(__builtin_constant_p(0), 0, (func, 1))
#else
#define CAN_CALL(func) 1
#endif

static int have_copy_file_range(void)
{
    int fd_in = -1;
    int fd_out = -1;

    char tmp_in[] = "/tmp/cfr_in.XXXXXX";
    char tmp_out[] = "/tmp/cfr_out.XXXXXX";

    fd_in = mkstemp(tmp_in);

    if (fd_in == -1)
    {
        goto cleanup;
    }

    fd_out = mkstemp(tmp_out);

    if (fd_out == -1)
    {
        goto cleanup;
    }

    if (write(fd_in, "test", 4) != 4)
    {
        goto cleanup;
    }

    off_t in_off = 0;
    off_t out_off = 0;

    ssize_t copied = copy_file_range(fd_in, &in_off, fd_out, &out_off, 4, 0);

    if (copied > 0)
    {
        close(fd_in);
        close(fd_out);
        unlink(tmp_in);
        unlink(tmp_out);
        return 1;
    }

cleanup:

    if (fd_in >= 0)
    {
        close(fd_in);
        unlink(tmp_in);
    }
    if (fd_out >= 0)
    {
        close(fd_out);
        unlink(tmp_out);
    }

    return 0;
}

static int have_sendfile(void)
{
    int fd_in = -1;
    int fd_out = -1;

    char tmp_in[] = "/tmp/sf_in.XXXXXX";
    char tmp_out[] = "/tmp/sf_out.XXXXXX";

    fd_in = mkstemp(tmp_in);

    if (fd_in == -1)
    {
        goto cleanup;
    }

    fd_out = mkstemp(tmp_out);

    if (fd_out == -1)
    {
        goto cleanup;
    }

    if (write(fd_in, "test", 4) != 4)
    {
        goto cleanup;
    }

    off_t off = 0;
    ssize_t sent = sendfile(fd_out, fd_in, &off, 4);

    if (sent > 0)
    {
        close(fd_in);
        close(fd_out);
        unlink(tmp_in);
        unlink(tmp_out);
        return 1;
    }

cleanup:

    if (fd_in >= 0)
    {
        close(fd_in);
        unlink(tmp_in);
    }
    if (fd_out >= 0)
    {
        close(fd_out);
        unlink(tmp_out);
    }

    return 0;
}

static int have_utimensat(void)
{
    char tmp[] = "/tmp/utimensat.XXXXXX";
    int fd = mkstemp(tmp);

    if (fd == -1)
    {
        return 0;
    }
    close(fd);

    struct timespec ts[2] = {{.tv_sec = 1000000000, .tv_nsec = 0}, {.tv_sec = 1000000000, .tv_nsec = 0}};

    int rc = utimensat(AT_FDCWD, tmp, ts, 0);
    unlink(tmp);
    return (rc == 0) ? 1 : 0;
}

int main(void)
{
    FILE *f = fopen("src/barr_config.h", "w");
    if (f == NULL)
    {
        perror("fopen");
        return 1;
    }
    fprintf(f, "/* barr_config.h - auto-generated */\n");
    fprintf(f, "# pragma once\n");
    fprintf(f, "#define BARR_HAVE_COPY_FILE_RANGE %d\n", have_copy_file_range());
    fprintf(f, "#define BARR_HAVE_SENDFILE %d\n", have_sendfile());
    fprintf(f, "#define BARR_HAVE_UTIMENSAT %d\n", have_utimensat());

    fclose(f);
    printf("[detect_features]: barr_config.h generated\n");
    return 0;
}

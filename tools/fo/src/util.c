#define _XOPEN_SOURCE 700

#include "util.h"

#include <ctype.h>
#include <errno.h>
#include <ftw.h>
#include <linux/limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

char *time_str(void)
{
    static __thread char buf[32];
    time_t now = time(NULL);
    struct tm tm_now;

    localtime_r(&now, &tm_now);
    strftime(buf, sizeof(buf), "%H:%M:%S", &tm_now);

    return buf;
}

bool is_dir(const char *dir)
{
    struct stat st;
    return (stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
}

bool is_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

const char *fo_getcwd(void)
{
    static char buf[1024];
    return getcwd(buf, sizeof(buf));
}

// caller must free
char *gtvalue(const char *fpath, const char *key)
{
    if (fpath == NULL || key == NULL)
    {
        return NULL;
    }

    FILE *fp = fopen(fpath, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    size_t key_len = strlen(key);
    char line[PATH_MAX];

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char *p = line;

        /* trim leading whitespace */
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        /* skip empty/comment lines */
        if (*p == '\0' || *p == '\n' || *p == '#')
        {
            continue;
        }

        /* check key match */
        if (strncmp(p, key, key_len) == 0)
        {
            char *q = p + key_len;

            /* skip spaces between key and = */
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }

            /* now expect '=' */
            if (*q != '=')
            {
                continue;
            }

            q++; /* skip '=' */

            /* skip whitespace before value */
            while (*q == ' ' || *q == '\t')
            {
                q++;
            }

            /* trim trailing newline */
            char *nl = strchr(q, '\n');
            if (nl != NULL)
            {
                *nl = '\0';
            }

            fclose(fp);
            return strdup(q);
        }
    }

    fclose(fp);
    return NULL;
}

int flwrite(const char *filename, const char *format, ...)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = vfprintf(fp, format, args);
    va_end(args);

    fflush(fp);
    fclose(fp);
    return result >= 0 ? 0 : -1;
}

int flappend(const char *filename, const char *format, ...)
{
    pthread_mutex_lock(&log_mutex);

    FILE *fp = fopen(filename, "a");
    if (!fp)
    {
        // errlog
        return -1;
    }

    va_list args;
    va_start(args, format);
    int result = vfprintf(fp, format, args);
    va_end(args);

    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&log_mutex);

    return result >= 0 ? 0 : -1;
}

//------------------------------------------------------------------------

static int remove_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    (void) sb;
    (void) ftwbuf;
    (void) typeflag;

    int err = remove(fpath);
    if (err)
    {
        // debug log
    }
    return err;
}

int rmrf(const char *path)
{
    if (access(path, F_OK) != 0)
    {
        // path does not exist, nothing to remove
        return 0;
    }

    // forbid bad raw strings immediately
    const char *forbidden_raw[] = {"/", ".", "..", "~", NULL};
    for (const char **p = forbidden_raw; *p; ++p)
    {
        if (strcmp(path, *p) == 0)
        {
            errno = EINVAL;
            return -1;
        }
    }

    // resolve to absolute path
    char resolved[PATH_MAX];
    if (!realpath(path, resolved))
    {
        return -1;  // invalid path
    }

    // forbid certain resolved absolute dirs
    const char *forbidden_abs[] = {"/", "/home", "/etc", "/usr", NULL};
    for (const char **p = forbidden_abs; *p; ++p)
    {
        if (strcmp(resolved, *p) == 0)
        {
            errno = EINVAL;
            return -1;
        }
    }

    return nftw(resolved, remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

void fo_notify(const char *summary, const char *body, const char *urgency, const char *icon)
{
    char *args[] = {"notify-send",
                    (char *) summary,
                    (char *) body,
                    urgency ? "-u" : NULL,
                    urgency ? (char *) urgency : NULL,
                    icon ? "-i" : NULL,
                    icon ? (char *) icon : NULL,
                    "-t",
                    "5000",
                    NULL};

    pid_t pid = fork();
    if (pid == 0)
    {
        execvp("notify-send", args);
        _exit(127);
    }
}

int run_process_blocking(const char *name, char **args)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(name, args);
        _exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, 0) == -1)
        {
            return -1;
        }

        if (WIFEXITED(status))
        {
            int code = WEXITSTATUS(status);
            if (code == 0)
            {
                return 0;
            }
            else
            {
                return code;
            }
        }
        else if (WIFSIGNALED(status))
        {
            return -1;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }
}

bool is_blank(const char *s)
{
    while (*s != '\0')
    {
        if (!isspace((unsigned char) *s))
        {
            return false;
        }
        s++;
    }
    return true;
}

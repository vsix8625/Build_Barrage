#define _XOPEN_SOURCE 700

#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xxh3.h>

barr_u64 BARR_hashstr(const char *s)
{
    if (s == NULL)
    {
        return 0;
    }
    return XXH64(s, strlen(s), 0);
}

bool BARR_hashcmp(barr_u64 h1, barr_u64 h2)
{
    return h1 == h2;
}

bool BARR_strmatch(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0;
}

void BARR_safecpy(char *dst, const char *src, size_t dst_size)
{
    if (dst_size == 0)
    {
        // debug log
        return;
    }

    strncpy(dst, src, dst_size);
    dst[dst_size - 1] = BARR_NULL_TERM_CHAR;
}

static barr_i32 remove_cb(const char *fpath, const struct stat *sb, barr_i32 typeflag, struct FTW *ftwbuf)
{
    (void) sb;
    (void) ftwbuf;
    (void) typeflag;

    barr_i32 err = remove(fpath);
    if (err)
    {
        // debug log
    }
    return err;
}

barr_i32 BARR_rmrf(const char *path)
{
    // forbid bad raw strings immediately
    const char *forbidden_raw[] = {"/", ".", "..", "~", NULL};
    for (const char **p = forbidden_raw; *p; ++p)
    {
        if (strcmp(path, *p) == 0)
        {
            errno = EINVAL;
            BARR_errlog("Refusing to rmrf forbidden raw path: %s", path);
            return -1;
        }
    }

    // resolve to absolute path
    char resolved[BARR_PATH_MAX];
    if (!realpath(path, resolved))
    {
        return -1;  // invalid path
    }

    // forbid certain resolved absolute dirs
    const char *forbidden_abs[] = {"/", BARR_GET_HOME(), "/home", "/etc", "/usr", NULL};
    for (const char **p = forbidden_abs; *p; ++p)
    {
        if (strcmp(resolved, *p) == 0)
        {
            errno = EINVAL;
            BARR_errlog("Refusing to rmrf forbidden abs path: %s", resolved);
            return -1;
        }
    }

    return nftw(resolved, remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

bool BARR_mv(const char *src, const char *dst)
{
    return rename(src, dst) == 0 ? true : false;
}

bool BARR_is_blank(const char *s)
{
    while (*s != '\0')
    {
        if (!isspace((barr_u8) *s))
        {
            return false;
        }
        s++;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------

static time_t barr_get_mtime(const char *path)
{
    if (path)
    {
        barr_stat_t st;
        return barr_stat(path, &st) == 0 ? st.st_mtime : 0;
    }
    return 0;
}

bool BARR_is_modified(const char *path, const char *prev_stamp_file)
{
    time_t curr = barr_get_mtime(path);
    time_t prev = barr_get_mtime(prev_stamp_file);

    if (prev == 0)
    {
        return true;
    }

    return curr != prev;
}

void BARR_update_Barrfile_stamp(void)
{
    FILE *f = fopen(BARRFILE_TIMESTAMP_PATH, "w");
    if (f)
    {
        time_t now = time(NULL);
        fprintf(f, "%ld\n", (long) now);
        fclose(f);
    }
}

bool BARR_is_src_newer(const char *src, const char *target)
{
    time_t src_t = barr_get_mtime(src);
    time_t target_t = barr_get_mtime(target);
    return src_t > target_t;
}

bool BARR_isdir(const char *dir)
{
    barr_stat_t st;
    return (barr_stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
}

bool BARR_isfile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

barr_i32 BARR_setperm(const char *path, const char *perm)
{
    if (BARR_strmatch(perm, "read-only"))
    {
        int failed = 0;

#if defined(BARR_OS_WIN32)
        failed = SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY) == 0;
#elif defined(BARR_OS_LINUX)
        failed = chmod(path, 0444) != 0;
#endif

        if (failed)
        {
            BARR_errlog("Failed to set %s to read-only", path);
            return 1;
        }
        else
        {
            BARR_dbglog("%s successfully set to read-only", path);
        }
    }

    // TODO: other permission modes

    return 0;
}

//----------------------------------------------------------------------------------------------------

char *BARR_which(const char *app)
{
    const char *env_path = getenv("PATH");
    if (env_path == NULL)
    {
        BARR_errlog("%s() failed to get $PATH env", __func__);
        return NULL;
    }

    char *path = strdup(env_path);
    if (!path)
    {
        return NULL;
    }

    char *saveptr = NULL;
    char *dir = strtok_r(path, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);

    while (dir)
    {
        char fullpath[BARR_BUF_SIZE_1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, app);
        if (barr_access(fullpath, X_OK) == 0)
        {
            free(path);
            return strdup(fullpath);  // caller owns the memory
        }
        dir = strtok_r(NULL, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);
    }

    free(path);
    return NULL;
}

bool BARR_is_installed(const char *app)
{
    const char *env_path = getenv("PATH");
    if (!env_path)
    {
        BARR_errlog("%s() failed to get $PATH env", __func__);
        return false;
    }

    char *path = strdup(env_path);
    char *saveptr = NULL;
    char *dir = strtok_r(path, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);

    while (dir)
    {
        char fullpath[BARR_BUF_SIZE_1024];
        // TODO: win32 version?
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, app);
        if (barr_access(fullpath, X_OK) == 0)
        {
            free(path);
            return true;
        }
        dir = strtok_r(NULL, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);
    }
    free(path);
    return false;
}

//----------------------------------------------------------------------------------------------------

bool BARR_isdir_empty(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
    {
        BARR_errlog("Cannot open %s directory", path);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL)
    {
        if (BARR_strmatch(entry->d_name, ".") || BARR_strmatch(entry->d_name, ".."))
        {
            continue;
        }
        else
        {
            closedir(d);
            return false;  // found something
        }
    }

    closedir(d);
    return true;  // empty only . and ..
}

//----------------------------------------------------------------------------------------------------

static size_t barr_count_tokens_in_string(const char *s)
{
    size_t count = 0;

    if (!s)
    {
        return 0;
    }

    const char *p = s;
    for (;;)
    {
        while ((*p != '\0') && isspace((barr_u8) *p))
        {
            ++p;
        }

        if (*p == '\0')
        {
            break;
        }

        ++count;

        while ((*p != '\0') && !isspace((barr_u8) *p))
        {
            ++p;
        }
    }

    return count;
}

static size_t barr_count_tokens_in_array(const char **arr)
{
    if (!arr)
    {
        return 0;
    }
    size_t total = 0;

    for (size_t i = 0; arr[i] != NULL; ++i)
    {
        total += barr_count_tokens_in_string(arr[i]);
    }
    return total;
}

static bool barr_token_exists_in_out(char **out, size_t n, const char *tok)
{
    for (size_t i = 0; i < n; ++i)
    {
        if (BARR_strmatch(out[i], tok))
        {
            return true;
        }
    }
    return false;
}

static void barr_tokenize_and_append_unique(char **out, size_t cap, size_t *out_count, const char *src)
{
    if (out == NULL || out_count == 0 || src == NULL)
    {
        return;
    }

    const char *p = src;

    while (*p != '\0')
    {
        while ((*p != '\0') && isspace((barr_u8) *p))
        {
            ++p;
        }

        if (*p == '\0')
        {
            break;
        }

        const char *start = p;

        while ((*p != '\0') && !isspace((unsigned char) *p))
        {
            ++p;
        }

        size_t len = (size_t) (p - start);

        if (len == 0)
        {
            break;
        }

        char *tok = BARR_gc_alloc(len + 1);
        if (!tok)
        {
            continue;
        }

        memcpy(tok, start, len);
        tok[len] = '\0';

        if (!barr_token_exists_in_out(out, *out_count, tok))
        {
            if (*out_count < cap)
            {
                out[*out_count] = tok;
                *out_count += 1;
            }
            else
            {
                break;
            }
        }
    }
}

const char *barr_normalize_include_flag(const char *flag)
{
    if (flag == NULL)
    {
        return NULL;
    }

    if (strncmp(flag, "-I", 2) != 0)
    {
        return BARR_gc_strdup(flag);
    }

    const char *path = flag + 2;
    char real[BARR_PATH_MAX];
    if (realpath(path, real) != NULL)
    {
        char buf[BARR_PATH_MAX + 3];
        snprintf(buf, sizeof(buf), "-I%s", real);
        return BARR_gc_strdup(buf);
    }

    // fallback: keep original if realpath fails
    return BARR_gc_strdup(flag);
}

const char **BARR_dedup_flags_array(const char **src_arr)
{
    if (!src_arr)
    {
        return NULL;
    }

    size_t total_tokens = barr_count_tokens_in_array(src_arr);

    if (total_tokens == 0)
    {
        const char **empty = BARR_gc_alloc(sizeof(char *));
        if (!empty)
        {
            return NULL;
        }
        ((char **) empty)[0] = NULL;
        return empty;
    }

    char **out = BARR_gc_alloc((total_tokens + 1) * sizeof(char *));
    if (!out)
    {
        return NULL;
    }

    size_t out_count = 0;

    for (size_t i = 0; src_arr[i] != NULL; ++i)
    {
        const char *normalized = barr_normalize_include_flag(src_arr[i]);
        barr_tokenize_and_append_unique(out, total_tokens, &out_count, normalized);
    }

    out[out_count] = NULL;

    return (const char **) out;
}

//----------------------------------------------------------------------------------------------------
// tokenize str

char **BARR_tokenize_string(const char *str)
{
    if (str == NULL)
    {
        return NULL;
    }

    size_t token_count = 0;
    const char *p = str;

    while (*p)
    {
        while (*p && isspace((barr_u8) *p))
        {
            p++;
        }

        if (!p)
        {
            break;
        }

        ++token_count;

        while (*p && !isspace((barr_u8) *p))
        {
            p++;
        }
    }

    char **arr = BARR_gc_calloc(token_count + 2, sizeof(char *));
    if (arr == NULL)
    {
        return NULL;
    }

    size_t idx = 0;
    const char *start = str;
    while (*start)
    {
        while (*start && isspace((barr_u8) *start))
        {
            ++start;
        }
        if (!*start)
        {
            break;
        }

        const char *end = start;
        while (*end && !isspace((barr_u8) *end))
        {
            ++end;
        }

        size_t len = (size_t) (end - start);
        char *tok = BARR_gc_alloc(len + 1);
        if (tok == NULL)
        {
            start = end;
            continue;
        }

        memcpy(tok, start, len);
        tok[len] = '\0';
        arr[idx++] = tok;

        start = end;
    }

    arr[idx] = NULL;
    return arr;
}

//----------------------------------------------------------------------------------------------------

static double barr_time_elapsed(const struct timespec *start, const struct timespec *end)
{
    if (start == NULL || end == NULL)
    {
        return 0.0;
    }

    barr_i64 sec = end->tv_sec - start->tv_sec;
    barr_i64 nsec = end->tv_nsec - start->tv_nsec;
    if (nsec < 0)
    {
        nsec += 1000000000L;
    }
    return (double) sec + (double) nsec * 1e-9;
}

const char *BARR_fmt_time_elapsed(const struct timespec *start, const struct timespec *end)
{
    if (start == NULL || end == NULL)
    {
        return NULL;
    }

    static char buf[BARR_BUF_SIZE_32];
    double s = barr_time_elapsed(start, end);
    if (s < 0)
    {
        s = 0;
    }

    if (s < 1e-3)
    {
        snprintf(buf, sizeof(buf), "%.0f µs", s * 1e6);
    }
    else if (s < 1.0)
    {
        snprintf(buf, sizeof(buf), "%.1f ms", s * 1e3);
    }
    else if (s < 60.0)
    {
        snprintf(buf, sizeof(buf), "%.2f s", s);
    }
    else
    {
        double m = s / 60.0;
        snprintf(buf, sizeof(buf), "%.2f min", m);
    }

    return buf;
}

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_mkdir_p(const char *path)
{
    char tmp[BARR_PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp))
    {
        return -1;
    }

    strncpy(tmp, path, sizeof(tmp));
    tmp[len] = '\0';

    for (char *p = tmp + 1; *p; ++p)
    {
        if (*p == BARR_PATH_SEPARATOR_CHAR)
        {
            *p = '\0';
            if (barr_mkdir(tmp) != 0)
            {
                if (errno != EEXIST)
                {
                    BARR_errlog("%s(): mkdir failed", __func__);
                    return -1;
                }
            }
            *p = BARR_PATH_SEPARATOR_CHAR;
        }
    }

    if (barr_mkdir(tmp) != 0)
    {
        if (errno != EEXIST)
        {
            BARR_errlog("%s(): mkdir failed", __func__);
            return -1;
        }
    }

    return 0;
}

//----------------------------------------------------------------------------------------------------

bool BARR_is_valid_tool(const char *tool)
{
    if (tool == NULL)
    {
        return false;
    }

    const char *args[] = {tool, "--version", NULL};
    barr_i32 res = BARR_run_process(args[0], (char **) args, false);
    if (res != 0)
    {
        return false;
    }
    return true;
}

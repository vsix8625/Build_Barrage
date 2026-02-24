#define _XOPEN_SOURCE 700

#include "barr_debug.h"
#include "barr_env.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "barr_list.h"

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

static barr_i32
remove_cb(const char *fpath, const struct stat *sb, barr_i32 typeflag, struct FTW *ftwbuf)
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
    if (barr_access(path, F_OK) != 0)
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

bool BARR_is_newer(const char *src, const char *target)
{
    time_t src_t    = barr_get_mtime(src);
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

char *BARR_find_in_path(const char *app)
{
    const char *env_path = getenv("PATH");
    if (env_path == NULL)
    {
        return NULL;
    }

    char *path    = BARR_gc_strdup(env_path);
    char *saveptr = NULL;
    char *dir = strtok_r(path, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);

    while (dir)
    {
        char fullpath[BARR_BUF_SIZE_1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, app);

        if (barr_access(fullpath, X_OK) == 0)
        {
            char *result = BARR_gc_strdup(fullpath);
            return result;
        }

        dir = strtok_r(NULL, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);
    }

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

    char *path    = strdup(env_path);
    char *saveptr = NULL;
    char *dir = strtok_r(path, (const char[]) {BARR_PATH_DELIMITER, BARR_NULL_TERM_CHAR}, &saveptr);

    while (dir)
    {
        char fullpath[BARR_BUF_SIZE_1024];
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

size_t BARR_count_tokens_in_array(const char **arr)
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

static void
barr_tokenize_and_append_unique(char **out, size_t cap, size_t *out_count, const char *src)
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
                out[*out_count]  = tok;
                *out_count      += 1;
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
    char        real[BARR_PATH_MAX];
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
    if (src_arr == NULL)
    {
        return NULL;
    }

    size_t total_tokens = BARR_count_tokens_in_array(src_arr);

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

    size_t      token_count = 0;
    const char *p           = str;

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

    size_t      idx   = 0;
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
        char  *tok = BARR_gc_alloc(len + 1);
        if (tok == NULL)
        {
            start = end;
            continue;
        }

        memcpy(tok, start, len);
        tok[len]   = '\0';
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

    barr_i64 sec  = end->tv_sec - start->tv_sec;
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
    double      s = barr_time_elapsed(start, end);
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
    char   tmp[BARR_PATH_MAX];
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

static const char *compiler_list[] =
    {"gcc", "g++", "clang", "clang++", "cc", "c++", "clang-18", "clang-17", "tcc", "zig", NULL};

bool BARR_is_valid_tool(const char *tool_path)
{
    if (tool_path == NULL)
    {
        return false;
    }

    char resolved_path[BARR_PATH_MAX];
    if (realpath(tool_path, resolved_path) == NULL)
    {
        return false;
    }

    char path_cp[BARR_PATH_MAX];
    strncpy(path_cp, resolved_path, BARR_PATH_MAX);
    path_cp[BARR_PATH_MAX - 1] = '\0';

    char *name = basename(path_cp);

    bool in_whitelist = false;
    for (barr_i32 i = 0; compiler_list[i] != NULL; i++)
    {
        if (strcmp(name, compiler_list[i]) == 0)
        {
            in_whitelist = true;
            break;
        }
    }

    if (!in_whitelist)
    {
        if (strstr(name, "gcc") || strstr(name, "clang") || strstr(name, "zig"))
        {
            in_whitelist = true;
        }
    }

    if (!in_whitelist)
    {
        return false;
    }

    const char *args[] = {tool_path, "--version", NULL};
    if (g_barr_verbose)
    {
        return BARR_run_process(args[0], (char **) args, false) == 0;
    }
    else
    {
        return BARR_run_process_dev_null(args[0], (char **) args) == 0;
    }
}

//----------------------------------------------------------------------------------------------------

void BARR_trim(char *s)
{
    char *start = s;
    char *end;

    while (isspace((barr_u8) *start))
    {
        start++;
    }

    if (*start == 0)
    {
        *s = 0;
        return;
    }

    end = start + strlen(start) - 1;
    while (end > start && isspace((barr_u8) *end))
    {
        end--;
    }
    *(end + 1) = '\0';

    if (start != s)
    {
        memmove(s, start, strlen(start) + 1);
    }
}

static char *barr_trim_s(char *str)
{
    while (isspace((unsigned char) *str))
    {
        str++;
    }
    if (*str == 0)
    {
        return str;
    }
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char) *end))
    {
        *end = 0;
        end--;
    }
    return str;
}

//-------------------------------------------------------------------------------

bool BARR_update_build_info_timestamp(const char *file_path)
{
    FILE *fp = fopen(file_path, "r+");

    if (fp == NULL)
    {
        return false;
    }

    char     line[BARR_BUF_SIZE_1024];
    size_t   ts_len  = strlen("timestamp");
    barr_i64 new_ts  = time(NULL);
    barr_i64 pos     = 0;
    bool     updated = false;

    while (fgets(line, sizeof(line), fp))
    {
        char *eq = strchr(line, '=');
        if (!eq)
        {
            pos = ftell(fp);
            continue;
        }

        *eq            = 0;
        char *line_key = barr_trim_s(line);

        if (strncmp(line_key, "timestamp", ts_len) == 0)
        {
            fseek(fp, pos, SEEK_SET);
            fprintf(fp, "timestamp = %ld\n", new_ts);
            updated = true;
            break;
        }

        pos = ftell(fp);
    }

    if (!updated)
    {
        // append if not found
        fseek(fp, 0, SEEK_END);
        fprintf(fp, "timestamp = %ld\n", new_ts);
    }

    fclose(fp);
    return true;
}

char *BARR_get_build_info_key(const char *file_path, const char *key)
{
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL)
    {
        return NULL;
    }

    char   line[BARR_BUF_SIZE_1024];
    size_t key_len = strlen(key);

    while (fgets(line, sizeof(line), fp))
    {
        char *eq = strchr(line, '=');
        if (!eq)
        {
            continue;
        }

        *eq = 0;

        char *line_key = barr_trim_s(line);
        char *line_val = barr_trim_s(eq + 1);

        if (strncmp(line_key, key, key_len) == 0 && strlen(line_key) == key_len)
        {
            fclose(fp);
            return BARR_gc_strdup(line_val);
        }
    }

    fclose(fp);
    return NULL;
}

//----------------------------------------------------------------------------------------------------

bool BARR_is_absolute(const char *p)
{
    if (p == NULL)
    {
        return false;
    }
#ifdef BARR_OS_LINUX
    return p && p[0] == '/';
#endif
    return false;
}

void BARR_join_path(char *out, size_t out_size, const char *base, const char *rel)
{
    if (base == NULL || rel == NULL)
    {
        return;
    }

    snprintf(out, out_size, "%s/%s", base, rel);
}

bool BARR_path_resolve(const char *base, const char *rel, char *out, size_t out_size)
{
    if (!base || !rel || !out)
        return false;

    if (BARR_is_absolute(rel))
    {
        strncpy(out, rel, out_size - 1);
        out[out_size - 1] = '\0';
        return true;
    }

    char temp[BARR_PATH_MAX];
    BARR_join_path(temp, sizeof(temp), base, rel);

    // Normalize the path (remove "./", "../" etc.)
    char *resolved = realpath(temp, NULL);
    if (resolved)
    {
        strncpy(out, resolved, out_size - 1);
        out[out_size - 1] = '\0';
        free(resolved);
        return true;
    }

    // fallback to joined path if realpath fails
    strncpy(out, temp, out_size - 1);
    out[out_size - 1] = '\0';
    return false;
}

char *BARR_get_self_exe(void)
{
    char    current_barr_path[BARR_PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", current_barr_path, sizeof(current_barr_path) - 1);
    if (len != 1)
    {
        current_barr_path[len] = '\0';
    }
    else
    {
        strncpy(current_barr_path, "barr", sizeof(current_barr_path));
    }

    return BARR_gc_strdup(current_barr_path);
}

//----------------------------------------------------------------------------------------------------
// Object scanner

static bool barr_is_object_file(const char *path)
{
    if (path == NULL)
    {
        return false;
    }

    size_t len = strlen(path);
    return len > 2 && BARR_strmatch(path + len - 2, ".o");
}

void BARR_object_files_scan(BARR_List *list, const char *dirpath)
{
    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        BARR_errlog("%s(): failed to open directory '%s'", __func__, dirpath);
        return;
    }

    struct dirent *dent;
    while ((dent = readdir(dir)))
    {
        if (BARR_strmatch(dent->d_name, ".") || BARR_strmatch(dent->d_name, ".."))
        {
            continue;
        }

        char fullpath[BARR_PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, dent->d_name);

        if (dent->d_type == DT_REG && barr_is_object_file(fullpath))
        {
            BARR_list_push(list, BARR_gc_strdup(fullpath));
        }
    }

    closedir(dir);
    if (g_barr_verbose)
    {
        BARR_log("Found: %zu objects files in '%s'", list->count, dirpath);
    }
}

bool BARR_isdigit_str(const char *s)
{
    if (s == NULL || !*s)
    {
        return false;
    }

    for (const char *p = s; *p; ++p)
    {
        if (!isdigit((barr_u8) *p))
        {
            return false;
        }
    }
    return true;
}

// -------------------------------------------------------------------------------

void BARR_scan_dir_shallow(BARR_List *list, const char *dirpath, BARR_ScanType type)
{
    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    DIR *dir = opendir(dirpath);
    if (dir == NULL)
    {
        BARR_warnlog("%s(): cannot open dir '%s'", __func__, dirpath);
    }

    struct dirent *ent;
    while ((ent = readdir(dir)))
    {
        if (BARR_strmatch(ent->d_name, ".") || BARR_strmatch(ent->d_name, ".."))
        {
            continue;
        }

        char *fullpath = BARR_gc_alloc(BARR_PATH_MAX);
        snprintf(fullpath, BARR_PATH_MAX, "%s/%s", dirpath, ent->d_name);

        if (ent->d_type == DT_DIR)
        {
            if (type == BARR_SCAN_TYPE_DIR || BARR_SCAN_TYPE_ALL)
            {
                BARR_list_push(list, fullpath);
            }
        }
        else if (ent->d_type == DT_REG)
        {
            if (type == BARR_SCAN_TYPE_FILE || BARR_SCAN_TYPE_ALL)
            {
                BARR_list_push(list, fullpath);
            }
        }
    }
    closedir(dir);
}

void BARR_scan_dir(BARR_List *list, const char *dirpath, BARR_ScanType type)
{
    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    char **queue = malloc(64 * sizeof(char *));
    if (queue == NULL)
    {
        BARR_errlog("%s(): failed to allocate memory for queue", __func__);
        return;
    }

    size_t que_size   = 0;
    size_t que_cap    = 64;
    queue[que_size++] = strdup(dirpath);

    while (que_size > 0)
    {
        char *current = queue[--que_size];
        if (!current)
        {
            continue;
        }

        DIR *dir = opendir(current);
        if (dir == NULL)
        {
            BARR_warnlog("%s(): cannot open dir '%s'", __func__, current);
            free(current);
            continue;
        }

        struct dirent *ent;
        while ((ent = readdir(dir)))
        {
            if (BARR_strmatch(ent->d_name, ".") || BARR_strmatch(ent->d_name, ".."))
            {
                continue;
            }

            char *fullpath = BARR_gc_alloc(BARR_PATH_MAX);
            snprintf(fullpath, BARR_PATH_MAX, "%s/%s", current, ent->d_name);

            if (ent->d_type == DT_DIR)
            {
                if (type == BARR_SCAN_TYPE_DIR || BARR_SCAN_TYPE_ALL)
                {
                    BARR_list_push(list, fullpath);
                }

                if (que_size >= que_cap)
                {
                    que_cap        *= 2;
                    char **new_que  = realloc(queue, que_cap * sizeof(char *));

                    if (new_que == NULL)
                    {
                        BARR_errlog("%s(): failed to realloc queue", __func__);
                        break;
                    }
                    queue = new_que;
                }
                queue[que_size++] = strdup(fullpath);
            }
            else if (ent->d_type == DT_REG)
            {
                if (type == BARR_SCAN_TYPE_FILE || BARR_SCAN_TYPE_ALL)
                {
                    BARR_list_push(list, fullpath);
                }
            }
        }

        closedir(dir);
        free(current);
    }

    free(queue);
}

static void BARR_fsinfo_fill(BARR_FSInfo *info, const char *path, const struct barr_stat *st)
{
    info->path = BARR_gc_strdup(path);
    info->size = (size_t) st->st_size;

    info->modified_time = st->st_mtime;
    info->modified_time = st->st_atime;

    info->hard_links = st->st_nlink;
    info->mode       = st->st_mode;

    info->owner_id  = st->st_uid;
    info->group_id  = st->st_gid;
    info->device_id = st->st_dev;

    info->blocks_allocated = st->st_blocks;
}

void BARR_fsinfo_collect_stats_dir_r(BARR_List *list, const char *dirpath)
{
    if (list == NULL || dirpath == NULL)
    {
        BARR_errlog("%s(): invalid args", __func__);
        return;
    }

    char **queue = malloc(64 * sizeof(char *));
    if (queue == NULL)
    {
        BARR_errlog("%s(): failed to allocate memory for queue", __func__);
        return;
    }

    size_t que_size   = 0;
    size_t que_cap    = 64;
    queue[que_size++] = strdup(dirpath);

    while (que_size > 0)
    {
        char *current = queue[--que_size];
        if (current == NULL)
        {
            continue;
        }

        DIR *dir = opendir(current);
        if (dir == NULL)
        {
            BARR_warnlog("%s(): cannot open dir '%s'", __func__, current);
            free(current);
            continue;
        }

        struct dirent *ent;

        while ((ent = readdir(dir)))
        {
            if (BARR_strmatch(ent->d_name, ".") || BARR_strmatch(ent->d_name, ".."))
            {
                continue;
            }

            char *fullpath = BARR_gc_alloc(BARR_PATH_MAX);
            snprintf(fullpath, BARR_PATH_MAX, "%s/%s", current, ent->d_name);

            struct barr_stat st;
            if (lstat(fullpath, &st) != 0)
            {
                BARR_warnlog("%s(): cannot stat '%s'", __func__, fullpath);
                continue;
            }

            BARR_FSInfo *info = malloc(sizeof(BARR_FSInfo));
            if (info == NULL)
            {
                BARR_errlog("%s(): failed to allocate fsinfo", __func__);
                continue;
            }

            BARR_fsinfo_fill(info, fullpath, &st);
            BARR_list_push(list, info);

            if (S_ISDIR(st.st_mode))
            {
                if (que_size >= que_cap)
                {
                    que_cap        *= 2;
                    char **new_que  = realloc(queue, que_cap * sizeof(char *));
                    if (new_que == NULL)
                    {
                        BARR_errlog("%s(): failed to realloc queue", __func__);
                        break;
                    }
                    queue = new_que;
                }

                queue[que_size++] = strdup(fullpath);
            }
        }

        closedir(dir);
        free(current);
    }

    free(queue);
}

static void barr_fsinfo_print(const BARR_FSInfo *info)
{
    if (info == NULL)
    {
        return;
    }

    printf("%s\n", info->path);
    printf("{\n");
    printf("    size            = %zu\n", info->size);
    printf("    modified_time   = %ld\n", (long) info->modified_time);
    printf("    accessed_time   = %ld\n", (long) info->accessed_time);
    printf("    owner_id        = %u\n", info->owner_id);
    printf("    group_id        = %u\n", info->group_id);
    printf("    mode            = %o\n", info->mode);
    printf("    hard_links      = %lu\n", (unsigned long) info->hard_links);
    printf("    device_id       = %lu\n", (unsigned long) info->device_id);
    printf("    blocks_allocated = %lu\n", (unsigned long) info->blocks_allocated);
    printf("}\n\n");
}

const char *BARR_bytes_to_human(size_t bytes, char *out, size_t out_size)
{
    if (bytes < 1024)
    {
        snprintf(out, out_size, "%zuB", bytes);
    }
    else if (bytes < 1024 * 1024)
    {
        snprintf(out, out_size, "%.2fKB", bytes / 1024.0);
    }
    else if (bytes < 1024 * 1024 * 1024)
    {
        snprintf(out, out_size, "%.2fMB", bytes / (1024.0 * 1024.0));
    }
    else
    {
        snprintf(out, out_size, "%.2fGB", bytes / (1024.0 * 1024.0 * 1024.0));
    }
    return out;
}

//----------------------------------------------------------------------------------------------------

typedef struct
{
    BARR_FSInfo *info;
} ExecEntry;

static barr_i32 barr_compare_exec_desc(const void *a, const void *b)
{
    const ExecEntry *ea = (const ExecEntry *) a;
    const ExecEntry *eb = (const ExecEntry *) b;
    if (ea->info->size < eb->info->size)
    {
        return 1;
    }
    else if (ea->info->size > eb->info->size)
    {
        return -1;
    }
    return 0;
}

//----------------------------------------------------------------------------------------------------

void BARR_get_relative_path(char *out, size_t out_size, const char *root, const char *full_path)
{
    size_t root_len = strlen(root);
    if (strncmp(full_path, root, root_len) == 0 && full_path[root_len] == '/')
    {
        // Path inside project: strip root + leading slash
        snprintf(out, out_size, "%s", full_path + root_len + 1);
    }
    else
    {
        // Outside project: just use basename
        char path_copy[PATH_MAX];
        strncpy(path_copy, full_path, sizeof(path_copy) - 1);
        path_copy[sizeof(path_copy) - 1] = '\0';
        snprintf(out, out_size, "%s", basename(path_copy));
    }
}

void BARR_list_fsinfo_print(const BARR_List *list)
{
    if (list == NULL)
    {
        return;
    }

    size_t       total_dirs     = 0;
    size_t       total_files    = 0;
    size_t       total_symlinks = 0;
    size_t       total_exec     = 0;
    size_t       total_size     = 0;
    BARR_FSInfo *largest        = NULL;

    for (size_t i = 0; i < list->count; ++i)
    {
        BARR_FSInfo *info = (BARR_FSInfo *) list->items[i];
        if (g_barr_verbose)
        {
            barr_fsinfo_print(info);
        }

        total_size += info->size;
        if (S_ISDIR(info->mode))
        {
            total_dirs++;
        }
        else if (S_ISREG(info->mode))
        {
            total_files++;
            if (info->mode & S_IXUSR)
            {
                total_exec++;
            }

            if (largest == NULL || info->size > largest->size)
            {
                largest = info;
            }
        }
        else if (S_ISLNK(info->mode))
        {
            total_symlinks++;
        }
    }

    printf("[summary]: Dirs %zu, Files %zu, Symlinks %zu, Executables %zu\n",
           total_dirs,
           total_files,
           total_symlinks,
           total_exec);
    char hsize[BARR_BUF_SIZE_32];
    BARR_bytes_to_human(total_size, hsize, sizeof(hsize));
    printf("[summary]: Total %zu, Size %s\n", list->count, hsize);

    if (largest)
    {
        char hsize_largest[32];
        BARR_bytes_to_human(largest->size, hsize_largest, sizeof(hsize_largest));
        printf("[largest]: %s: \033[34m%s\033[0m\n", largest->path, hsize_largest);
    }

    if (total_exec > 0)
    {
        ExecEntry *execs = BARR_gc_alloc(sizeof(ExecEntry) * total_exec);

        if (execs == NULL)
        {
            return;
        }

        size_t idx = 0;

        for (size_t i = 0; i < list->count; ++i)
        {
            BARR_FSInfo *info = (BARR_FSInfo *) list->items[i];
            if (S_ISREG(info->mode) && (info->mode & S_IXUSR))
            {
                execs[idx++].info = info;
            }
        }

        qsort(execs, total_exec, sizeof(ExecEntry), barr_compare_exec_desc);

        printf("[executables]:\n");
        for (size_t i = 0; i < total_exec; ++i)
        {
            char hsize_exec[32];
            BARR_bytes_to_human(execs[i].info->size, hsize_exec, sizeof(hsize_exec));

            char rel_path[BARR_PATH_MAX];
            BARR_get_relative_path(rel_path, sizeof(rel_path), BARR_getcwd(), execs[i].info->path);

            printf("\t%-64s: \033[34m%s\033[0m\n", rel_path, hsize_exec);
        }
    }
}

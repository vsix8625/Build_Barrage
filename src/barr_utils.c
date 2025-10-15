#define _XOPEN_SOURCE 700

#include "barr_debug.h"
#include "barr_io.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool BARR_strmatch(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0 ? true : false;
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

bool BARR_is_modified(const char *path1, const char *path2)
{
    time_t t1 = barr_get_mtime(path1);
    time_t t2 = barr_get_mtime(path2);
    return (t1 != t2) ? true : false;
}

bool BARR_is_src_newer(const char *src, const char *target)
{
    time_t src_t = barr_get_mtime(src);
    time_t target_t = barr_get_mtime(target);
    return (src_t > target_t) ? true : false;
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
            closedir(d);
            return false;  // found something
        }
    }
    closedir(d);
    return true;  // empty only . and ..
}

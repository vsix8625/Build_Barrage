#define _XOPEN_SOURCE 700

#include "atl_io.h"

#include <ctype.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>

bool ATL_strmatch(const char *s1, const char *s2)
{
    return strcmp(s1, s2) == 0 ? true : false;
}

void ATL_safecpy(char *dst, const char *src, size_t dst_size)
{
    if (dst_size == 0)
    {
        // debug log
        return;
    }

    strncpy(dst, src, dst_size);
    dst[dst_size - 1] = ATL_NULL_TERM_CHAR;
}

static atl_i32 remove_cb(const char *fpath, const struct stat *sb, atl_i32 typeflag, struct FTW *ftwbuf)
{
    (void) sb;
    (void) ftwbuf;
    (void) typeflag;

    atl_i32 err = remove(fpath);
    if (err)
    {
        // debug log
    }
    return err;
}

atl_i32 ATL_rmrf(const char *path)
{
    return nftw(path, remove_cb, 64, FTW_DEPTH | FTW_PHYS);
}

bool ATL_mv(const char *src, const char *dst)
{
    return rename(src, dst) == 0 ? true : false;
}

bool ATL_is_blank(const char *s)
{
    while (*s != '\0')
    {
        if (!isspace((atl_u8) *s))
        {
            return false;
        }
        s++;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------

static time_t atl_get_mtime(const char *path)
{
    if (path)
    {
        atl_stat_t st;
        return atl_stat(path, &st) == 0 ? st.st_mtime : 0;
    }
    return 0;
}

bool ATL_is_modified(const char *path1, const char *path2)
{
    time_t t1 = atl_get_mtime(path1);
    time_t t2 = atl_get_mtime(path2);
    return (t1 != t2) ? true : false;
}

bool ATL_is_src_newer(const char *src, const char *target)
{
    time_t src_t = atl_get_mtime(src);
    time_t target_t = atl_get_mtime(target);
    return (src_t > target_t) ? true : false;
}

bool ATL_isdir(const char *dir)
{
    atl_stat_t st;
    return (atl_stat(dir, &st) == 0 && S_ISDIR(st.st_mode));
}

bool ATL_isfile(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

atl_i32 ATL_setperm(const char *path, const char *perm)
{
    if (ATL_strmatch(perm, "read-only"))
    {
        int failed = 0;

#if defined(ATL_OS_WIN32)
        failed = SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY) == 0;
#elif defined(ATL_OS_LINUX)
        failed = chmod(path, 0444) != 0;
#endif

        if (failed)
        {
            ATL_errlog("Failed to set %s to read-only", path);
            return 1;
        }
        else
        {
            ATL_log("%s successfully set to read-only", path);
        }
    }

    // TODO: other permission modes

    return 0;
}

#define _XOPEN_SOURCE 700

#include "atl_utils.h"

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

//----------------------------------------------------------------------------------------------------

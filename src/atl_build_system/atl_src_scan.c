#define _XOPEN_SOURCE 700

#include "atl_src_scan.h"
#include "atl_debug.h"
#include "atl_io.h"

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ATL_SourceList *g_scan_list = NULL;

static inline atl_i32 atl_is_source_file(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (!ext)
    {
        return 0;
    }

    return ATL_strmatch(ext, ".c") || ATL_strmatch(ext, ".cpp");
}

static atl_i32 nftw_scan_cb(const char *fpath, const struct stat *sb, atl_i32 type_flag, struct FTW *ftwbuf)
{
    ATL_VOID(sb);
    ATL_VOID(ftwbuf);
    if (type_flag == FTW_F && atl_is_source_file(fpath))
    {
        ATL_source_list_push(g_scan_list, fpath);
    }
    return 0;
}

void ATL_source_list_scan_dir(ATL_SourceList *list, const char *dirpath, const char *project_root)
{
    if (!list || !dirpath)
    {
        ATL_errlog("Failed: %d:%s", __LINE__, __FILE__);
        return;
    }

    g_scan_list = list;
    ATL_dbglog("Scanning: %s", dirpath);

    if (nftw(dirpath, nftw_scan_cb, ATL_NFTW_SOFT_CAP, FTW_PHYS) == -1)
    {
        ATL_errlog("nftw failed on %s", dirpath);
    }

    g_scan_list = NULL;

    for (size_t i = 0; i < list->count; i++)
    {
        char buf[ATL_PATH_MAX];
        if (project_root)
        {
            snprintf(buf, sizeof(buf), "%s/%s", project_root, list->entries[i]);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%s", list->entries[i]);
        }

        char *copy = strdup(buf);
        if (!copy)
        {
            ATL_errlog("%s(): failed to strdup path", __func__);
            continue;
        }
        free(list->entries[i]);
        list->entries[i] = copy;
    }
}

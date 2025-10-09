#include "atl_depgen.h"
#include "atl_cmd_version.h"
#include "atl_debug.h"
#include "atl_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: add this step in the compilation stage
void ATL_sources_get_deps(const ATL_SourceList *list)
{
    if (!list)
    {
        ATL_errlog("ATL_SourceLists is NULL");
        ATL_dumb_backtrace();
        return;
    }
    if (!ATL_is_initialized())
    {
        return;
    }
    if (!ATL_isdir(ATL_MARKER_DIR))
    {
        ATL_errlog("Could not detect project root directory");
        return;
    }

    if (!ATL_isdir("build/deps"))
    {
        if (atl_mkdir("build/deps"))
        {
            ATL_errlog("Could not create build/deps directory");
            ATL_dumb_backtrace();
            return;
        }
    }

    for (size_t i = 0; i < list->count; i++)
    {
        const char *src = list->entries[i];

        const char *fname = strrchr(src, '/');
        fname = fname ? fname + 1 : src;

        char depfile[ATL_BUF_SIZE_512];
        snprintf(depfile, sizeof(depfile), "build/deps/%s.d", fname);

        char out_file[1024];
        snprintf(out_file, sizeof(out_file), "build/%s.o", fname);

        char *flags = "-Werror";

        char *args[] = {"gcc", "-c", (char *) src, "-o", out_file, flags, "-MMD", "-MF", depfile, NULL};
        ATL_run_process(args[0], args, 0);
    }
}

#include "barr_linker.h"
#include "barr_io.h"

#include <stdlib.h>

barr_i32 BARR_archive_stage(BARR_SourceList *list, const char *archive_path)
{
    size_t argc = list->count + 3;
    char **args = malloc(sizeof(char *) * (argc + 1));
    size_t idx = 0;
    args[idx++] = "ar";
    args[idx++] = "rcs";
    args[idx++] = (char *) archive_path;

    for (size_t i = 0; i < list->count; ++i)
    {
        args[idx++] = list->entries[i];
    }
    args[idx] = NULL;

    barr_i32 ret = BARR_run_process(args[0], args, false);
    free(args);
    return ret;
}

barr_i32 BARR_link_stage(char **link_args)
{
    return BARR_run_process(link_args[0], link_args, false);
}

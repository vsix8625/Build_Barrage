#include "barr_linker.h"
#include "barr_debug.h"
#include "barr_gc.h"
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

// ----------------------------------------------------------------------------------------------------

BARR_LinkArgs *BARR_link_args_create(void)
{
    BARR_LinkArgs *la = BARR_gc_alloc(sizeof(BARR_LinkArgs));
    la->capacity = BARR_INITIAL_LINK_ARGS_CAPACITY;
    la->count = 0;
    la->args = BARR_gc_alloc(sizeof(char *) * la->capacity);
    la->args[0] = NULL;
    return la;
}

void BARR_link_args_add(BARR_LinkArgs *la, const char *arg)
{
    if (!la || !arg)
    {
        return;
    }

    if (la->count >= la->capacity)
    {
        la->capacity *= 2;
        la->args = BARR_gc_realloc(la->args, la->capacity);
    }

    la->args[la->count++] = (char *) arg;
    la->args[la->count] = NULL;
}

char **BARR_link_args_finalize(BARR_LinkArgs *la)
{
    if (!la)
    {
        return NULL;
    }
    la->args[la->count] = NULL;
    return la->args;
}

void BARR_dedup_libs_and_add_to_link_args(BARR_LinkArgs *la, const char **libs_array)
{
    if (!la || !libs_array)
    {
        return;
    }

    BARR_dbglog("pre dedubp flags_array");
    const char **deduped = BARR_dedup_flags_array(libs_array);
    if (!deduped)
    {
        return;
    }

    for (size_t i = 0; deduped[i] != NULL; ++i)
    {
        BARR_link_args_add(la, deduped[i]);
    }
}

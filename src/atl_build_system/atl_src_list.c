#include "atl_src_list.h"
#include "atl_io.h"

void ATL_source_list_init(ATL_SourceList *list, ATL_Arena *arena, size_t max_files)
{
    list->arena = arena;
    list->entries = ATL_arena_alloc(arena, max_files * sizeof(char *));
    list->count = 0;
    list->capacity = max_files;
}

void ATL_source_list_push(ATL_SourceList *list, const char *path)
{
    if (list->count >= list->capacity)
    {
        ATL_errlog("ATL_source_list overflow (count=%zu, max=%zu)", list->count, list->capacity);
        return;
    }

    list->entries[list->count++] = ATL_arena_strdup(list->arena, path);
}

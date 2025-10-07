#ifndef ATL_SRC_LIST_H_
#define ATL_SRC_LIST_H_

#include "atl_arena.h"
#include "atl_defs.h"

typedef struct ATL_SourceList
{
    char **entries;
    size_t count;
    size_t capacity;
    ATL_Arena *arena;
} ATL_SourceList;

void ATL_source_list_init(ATL_SourceList *list, ATL_Arena *arena, size_t max_files);
void ATL_source_list_push(ATL_SourceList *list, const char *path);

#endif  // ATL_SRC_LIST_H_

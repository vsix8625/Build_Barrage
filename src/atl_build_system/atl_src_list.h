#ifndef ATL_SRC_LIST_H_
#define ATL_SRC_LIST_H_

#include "atl_defs.h"

typedef struct ATL_SourceList
{
    char **entries;
    size_t count;
    size_t capacity;
} ATL_SourceList;

bool ATL_source_list_init(ATL_SourceList *list, size_t initial_file_cap);
bool ATL_source_list_push(ATL_SourceList *list, const char *path);
bool ATL_destroy_source_list(ATL_SourceList *list);

#endif  // ATL_SRC_LIST_H_

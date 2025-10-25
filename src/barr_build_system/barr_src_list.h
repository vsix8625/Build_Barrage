#ifndef BARR_SRC_LIST_H_
#define BARR_SRC_LIST_H_

#include "barr_defs.h"
#include "barr_thread_pool.h"

typedef struct BARR_SourceList
{
    char **entries;
    size_t count;
    size_t capacity;

    char *path_buffer;
    size_t path_buffer_size;
    size_t path_buffer_used;
} BARR_SourceList;

bool BARR_source_list_init(BARR_SourceList *list, size_t initial_file_cap);
bool BARR_source_list_push(BARR_SourceList *list, const char *path);
void BARR_destroy_source_list(BARR_SourceList *list);

#endif  // BARR_SRC_LIST_H_

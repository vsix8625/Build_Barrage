#ifndef BARR_LIST_H_
#define BARR_LIST_H_

#include "barr_defs.h"

typedef struct BARR_List
{
    void **items;
    size_t count;
    size_t capacity;
} BARR_List;

bool BARR_list_init(BARR_List *list, size_t initial_cap);
void BARR_list_push(BARR_List *list, void *item);

#endif  // BARR_LIST_H_

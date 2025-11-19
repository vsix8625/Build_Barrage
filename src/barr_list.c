#include "barr_list.h"
#include "barr_gc.h"
#include "barr_io.h"

bool BARR_list_init(BARR_List *list, size_t initial_cap)
{
    if (!list)
    {
        return false;
    }

    list->items = BARR_gc_alloc(sizeof(void *) * initial_cap);
    if (!list->items)
    {
        return false;
    }

    list->count = 0;
    list->capacity = initial_cap;

    return true;
}

void BARR_list_push(BARR_List *list, void *item)
{
    if (!list)
    {
        return;
    }

    if (list->count >= list->capacity)
    {
        size_t new_cap = list->capacity * 2;
        void **new_items = BARR_gc_realloc(list->items, sizeof(void *) * new_cap);
        if (!new_items)
        {
            return;
        }
        list->items = new_items;
        list->capacity = new_cap;
    }

    list->items[list->count++] = item;
}

void BARR_list_print(BARR_List *list)
{
    if (list == NULL)
    {
        BARR_errlog("NULL list");
        return;
    }

    for (size_t i = 0; i < list->count; ++i)
    {
        BARR_printf("\t[%zu]: %s\n", i + 1, (char *) list->items[i]);
    }
}

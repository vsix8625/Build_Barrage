
#ifndef ATL_ARENA_H_
#define ATL_ARENA_H_

#include "atl_defs.h"

#define ATL_ALLOC malloc

#define ATL_ARENA_MAGIC_START 0xBEEFBABE
#define ATL_ARENA_MAGIC_END 0xDEADBEEF

typedef struct ATL_Arena
{
    atl_u32 magic_start;

    char *buffer;
    size_t capacity;
    size_t offset;

    atl_u32 magic_end;
} ATL_Arena;

void ATL_arena_init(ATL_Arena *a, size_t capacity);
atl_ptr ATL_arena_alloc(ATL_Arena *a, size_t size);
void ATL_arena_reset(ATL_Arena *a);
void ATL_destroy_arena(ATL_Arena *a);
void ATL_arena_stats(const ATL_Arena *a);

#endif  // ATL_ARENA_H_

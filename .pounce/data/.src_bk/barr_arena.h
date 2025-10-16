
#ifndef BARR_ARENA_H_
#define BARR_ARENA_H_

#include "barr_defs.h"

#define BARR_ALLOC malloc

#define BARR_ARENA_MAGIC_START 0xBEEFBABE
#define BARR_ARENA_MAGIC_END 0xDEADBEEF

typedef struct BARR_Arena
{
    barr_u32 magic_start;

    char *buffer;
    size_t capacity;
    size_t offset;
    size_t peak;
    size_t alignment;
    char name[BARR_BUF_SIZE_256];

    barr_u32 magic_end;
} BARR_Arena;

void BARR_arena_init(BARR_Arena *a, size_t capacity, const char *name, size_t align);
barr_ptr BARR_arena_alloc(BARR_Arena *a, size_t size);
char *BARR_arena_strdup(BARR_Arena *a, const char *s);
void BARR_arena_reset(BARR_Arena *a);
void BARR_destroy_arena(BARR_Arena *a);
void BARR_arena_stats(const BARR_Arena *a);

#endif  // BARR_ARENA_H_

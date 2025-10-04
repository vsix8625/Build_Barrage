#include "atl_arena.h"
#include "atl_debug.h"
#include "atl_io.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static inline size_t atl_arena_free_space(const ATL_Arena *a)
{
    return a ? (a->capacity - a->offset) : 0;
}

void ATL_arena_init(ATL_Arena *a, size_t capacity, const char *name)
{
    a->magic_start = ATL_ARENA_MAGIC_START;
    a->magic_end = ATL_ARENA_MAGIC_END;

    a->buffer = (char *) ATL_ALLOC(capacity);
    a->capacity = capacity;
    a->offset = 0;
    a->peak = 0;
    a->name = name ? name : "unnamed";

    ATL_dbglog("%s: initialized %s | Address: %p", __func__, a->name, a);
    if (capacity < 1024)
    {
        ATL_dbglog("Total Size: %zub", capacity);
    }
    else if (capacity < 1024 * 1024)
    {
        ATL_dbglog("Total Size: %.2fKB", (float) capacity / 1024.0f);
    }
    else
    {
        ATL_dbglog("Total Size: %.2fMB", (float) capacity / (1024.0f * 1024.0f));
    }
}

atl_ptr ATL_arena_alloc(ATL_Arena *a, size_t size)
{
    ATL_dbglog("%s() request for %zu bytes from arena: %s-%p", __func__, size, a->name, a);
#if defined(ATL_DEBUG)
    assert(a->magic_start == ATL_ARENA_MAGIC_START && a->magic_end == ATL_ARENA_MAGIC_END);
#endif

    size_t free_space = atl_arena_free_space(a);
    if (size > free_space)
    {
        ATL_errlog("Arena out of memory: %p", a);
        ATL_errlog("Requested: %zu", size);
        ATL_errlog("Available: %zu", free_space);
        return NULL;
    }

    atl_ptr ptr = a->buffer + a->offset;
    a->offset += size;
    if (a->offset > a->peak)
    {
        a->peak = a->offset;
    }
    ATL_dbglog("Succesfully allocated: %zu bytes in %s:%p", size, a->name, a);
    return ptr;
}

char *ATL_arena_strdup(ATL_Arena *a, const char *s)
{
#if defined(ATL_DEBUG)
    assert(a->magic_start == ATL_ARENA_MAGIC_START && a->magic_end == ATL_ARENA_MAGIC_END);
#endif

    size_t len = strlen(s) + 1;
    char *dest = ATL_arena_alloc(a, len);
    if (!dest)
    {
        return NULL;
    }
    memcpy(dest, s, len);
    return dest;
}

void ATL_arena_reset(ATL_Arena *a)
{
    ATL_dbglog("%s(): arena %s-%p", __func__, a->name, a);
#if defined(ATL_DEBUG)
    assert(a->magic_start == ATL_ARENA_MAGIC_START && a->magic_end == ATL_ARENA_MAGIC_END);
#endif

    a->offset = 0;
}

void ATL_destroy_arena(ATL_Arena *a)
{
    ATL_dbglog("%s(): arena %s-%p", __func__, a->name, a);
#if defined(ATL_DEBUG)
    assert(a->magic_start == ATL_ARENA_MAGIC_START && a->magic_end == ATL_ARENA_MAGIC_END);
#endif

    if (a && a->buffer)
    {
        free(a->buffer);
        a->buffer = NULL;
        a->capacity = 0;
        a->offset = 0;
    }
    ATL_dbglog("Arena %s destroyed: %p", a->name, a);
}

static void atl_print_human_size(const char *label, size_t bytes)
{
    if (bytes < 1024)
        ATL_log("%s: %zub", label, bytes);
    else if (bytes < 1024 * 1024)
        ATL_log("%s: %.2fKB", label, (double) bytes / 1024.0);
    else if (bytes < 1024ull * 1024ull * 1024ull)
        ATL_log("%s: %.2fMB", label, (double) bytes / (1024.0 * 1024.0));
    else
        ATL_log("%s: %.2fGB", label, (double) bytes / (1024.0 * 1024.0 * 1024.0));
}

void ATL_arena_stats(const ATL_Arena *a)
{
#if defined(ATL_DEBUG)
    assert(a->magic_start == ATL_ARENA_MAGIC_START && a->magic_end == ATL_ARENA_MAGIC_END);
#endif

    size_t used = a->offset;
    size_t free_space = a->capacity - a->offset;
    double percent = (a->capacity > 0) ? (100.0 * used / (double) a->capacity) : 0.0;

    ATL_log("Arena Stats [%s-%p]: Usage: %.2f%%", a->name, a, percent);
    atl_print_human_size("\tUsed", used);
    atl_print_human_size("\tPeak", a->peak);
    atl_print_human_size("\tFree", free_space);
    atl_print_human_size("\tCapacity", a->capacity);
}

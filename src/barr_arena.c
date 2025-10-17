#include "barr_arena.h"
#include "barr_debug.h"
#include "barr_io.h"

#include <assert.h>
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline size_t barr_arena_free_space(const BARR_Arena *a)
{
    return a ? (a->capacity - a->offset) : 0;
}

size_t BARR_align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

void BARR_arena_init(BARR_Arena *a, size_t capacity, const char *name, size_t align)
{
    if (!a)
    {
        return;
    }
    a->magic_start = BARR_ARENA_MAGIC_START;
    a->magic_end = BARR_ARENA_MAGIC_END;

    a->buffer = (char *) BARR_ALLOC(capacity);
    if (!a->buffer)
    {
        BARR_errlog("Failed to allocate for %s:%p", name, a->buffer);
        BARR_dumb_backtrace();
        return;
    }

    memset(a->buffer, 0, capacity);

    a->capacity = capacity;
    a->offset = 0;
    a->peak = 0;
    BARR_safecpy(a->name, name ? name : "unnamed", sizeof(a->name));

    align = align ? align : 8;
    if (align < 8)
    {
        align = 8;
    }
    if (align > 64)
    {
        align = 64;
    }
    a->alignment = align;

    BARR_dbglog("%s: initialized %s | Address: %p", __func__, a->name, a);
    if (capacity < 1024)
    {
        BARR_dbglog("Total Size: %zub", capacity);
    }
    else if (capacity < 1024 * 1024)
    {
        BARR_dbglog("Total Size: %.2fKB", (float) capacity / 1024.0f);
    }
    else
    {
        BARR_dbglog("Total Size: %.2fMB", (float) capacity / (1024.0f * 1024.0f));
    }
}

barr_ptr BARR_arena_alloc(BARR_Arena *a, size_t size)
{
    if (!a || !a->buffer)
    {
        return NULL;
    }
//    BARR_dbglog("%s() request for %zu bytes from arena: %s-%p", __func__, size, a->name, a);
#if defined(BARR_DEBUG)
    assert(a->magic_start == BARR_ARENA_MAGIC_START && a->magic_end == BARR_ARENA_MAGIC_END);
#endif

    size_t aligned_size = BARR_align_up(size, a->alignment);
    size_t free_space = barr_arena_free_space(a);
    if (aligned_size > free_space)
    {
        BARR_errlog("Arena %s:%p is out of memory", a->name, a);
        BARR_errlog("Requested: %zu", aligned_size);
        BARR_errlog("Available: %zu", free_space);
        return NULL;
    }

    barr_ptr ptr = a->buffer + a->offset;
    a->offset += aligned_size;
    if (a->offset > a->peak)
    {
        a->peak = a->offset;
    }
    //   BARR_dbglog("Success");
    return ptr;
}

char *BARR_arena_strdup(BARR_Arena *a, const char *s)
{
    if (!a)
    {
        return NULL;
    }
#if defined(BARR_DEBUG)
    assert(a->magic_start == BARR_ARENA_MAGIC_START && a->magic_end == BARR_ARENA_MAGIC_END);
#endif

    size_t len = strlen(s) + 1;
    char *dest = BARR_arena_alloc(a, len);
    if (!dest)
    {
        return NULL;
    }
    memcpy(dest, s, len);
    return dest;
}

void BARR_arena_reset(BARR_Arena *a)
{
    //  BARR_dbglog("%s(): arena %s-%p", __func__, a->name, a);
#if defined(BARR_DEBUG)
    assert(a->magic_start == BARR_ARENA_MAGIC_START && a->magic_end == BARR_ARENA_MAGIC_END);
#endif

    a->offset = 0;
}

void BARR_destroy_arena(BARR_Arena *a)
{
    if (!a)
    {
        return;
    }
#if defined(BARR_DEBUG)
    assert(a->magic_start == BARR_ARENA_MAGIC_START && a->magic_end == BARR_ARENA_MAGIC_END);
#endif

    free(a->buffer);
    a->buffer = NULL;
    a->capacity = 0;
    a->offset = 0;
    BARR_dbglog("Arena %s destroyed: %p", a->name, a);
}

static void barr_print_human_size(const char *label, size_t bytes)
{
    if (bytes < 1024)
        BARR_log("%s: %zub", label, bytes);
    else if (bytes < 1024 * 1024)
        BARR_log("%s: %.2fKB", label, (double) bytes / 1024.0);
    else if (bytes < 1024ull * 1024ull * 1024ull)
        BARR_log("%s: %.2fMB", label, (double) bytes / (1024.0 * 1024.0));
    else
        BARR_log("%s: %.2fGB", label, (double) bytes / (1024.0 * 1024.0 * 1024.0));
}

void BARR_arena_stats(const BARR_Arena *a)
{
#if defined(BARR_DEBUG)
    assert(a->magic_start == BARR_ARENA_MAGIC_START && a->magic_end == BARR_ARENA_MAGIC_END);
#endif

    size_t used = a->offset;
    size_t free_space = a->capacity - a->offset;
    double percent = (a->capacity > 0) ? (100.0 * used / (double) a->capacity) : 0.0;

    BARR_log("Arena Stats [%s-%p]: Usage: %.2f%%", a->name, a, percent);
    barr_print_human_size("\tUsed", used);
    barr_print_human_size("\tPeak", a->peak);
    barr_print_human_size("\tFree", free_space);
    barr_print_human_size("\tCapacity", a->capacity);
}

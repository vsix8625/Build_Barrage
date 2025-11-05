#include "barr_gc.h"
#include "barr_debug.h"
#include "barr_env.h"
#include "barr_io.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static BARR_GC_List g_barr_gc_list = {0};

static bool barr_gc_grow(void)
{
    size_t new_capacity = g_barr_gc_list.capacity ? g_barr_gc_list.capacity * 2 : BARR_BUF_SIZE_2048;
    BARR_GC_Info *new_items = realloc(g_barr_gc_list.items, new_capacity * sizeof(BARR_GC_Info));
    if (!new_items)
    {
        BARR_errlog("%s(): failed to grow to %zu", __func__, new_capacity);
        return false;
    }

    g_barr_gc_list.items = new_items;
    g_barr_gc_list.capacity = new_capacity;

    //    BARR_dbglog("g_barr_gc_list new cap = %zu", new_capacity);
    return true;
}

static void barr_gc_push(void *ptr, size_t size, const char *fn, const char *file, barr_i32 line)
{
    if (ptr == NULL)
    {
        return;
    }

    fn = fn ? fn : "N/A";
    file = file ? file : "N/A";

    pthread_mutex_lock(&g_barr_gc_list.lock);

    if (g_barr_gc_list.count >= g_barr_gc_list.capacity)
    {
        if (!barr_gc_grow())
        {
            pthread_mutex_unlock(&g_barr_gc_list.lock);
            BARR_errlog("%s(): failed to grow GC list", __func__);
            return;
        }
    }

    BARR_GC_Info *info = &g_barr_gc_list.items[g_barr_gc_list.count++];
    info->ptr = ptr;
    info->fn = fn;
    info->size = size;
    info->file = file;
    info->line = line;

    pthread_mutex_unlock(&g_barr_gc_list.lock);
}

static void barr_gc_pop(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    pthread_mutex_lock(&g_barr_gc_list.lock);

    for (size_t i = g_barr_gc_list.count; i-- > 0;)
    {
        if (g_barr_gc_list.items[i].ptr == ptr)
        {
            g_barr_gc_list.items[i] = g_barr_gc_list.items[g_barr_gc_list.count - 1];
            g_barr_gc_list.count--;
            pthread_mutex_unlock(&g_barr_gc_list.lock);
            return;
        }
    }

    BARR_errlog("%s(): unknown free %p", __func__, ptr);
    pthread_mutex_unlock(&g_barr_gc_list.lock);
}

bool BARR_gc_init(void)
{
    memset(&g_barr_gc_list, 0, sizeof(g_barr_gc_list));
    if (pthread_mutex_init(&g_barr_gc_list.lock, NULL) != 0)
    {
        BARR_errlog("%s(): failed to init mutex", __func__);
        return false;
    }

    BARR_dbglog("Garbage collector initialized");
    return true;
}

void BARR_gc_shutdown(void)
{
    if (g_barr_gc_list.count && g_barr_verbose)
    {
        BARR_log("Garbage collector cleared (%zu) allocations", g_barr_gc_list.count);
    }
    pthread_mutex_lock(&g_barr_gc_list.lock);

    for (size_t i = g_barr_gc_list.count; i-- > 0;)
    {
        free(g_barr_gc_list.items[i].ptr);
    }
    free(g_barr_gc_list.items);

    g_barr_gc_list.items = NULL;
    g_barr_gc_list.count = 0;
    g_barr_gc_list.capacity = 0;

    pthread_mutex_unlock(&g_barr_gc_list.lock);
    pthread_mutex_destroy(&g_barr_gc_list.lock);
}

// ----------------------------------------------------------------------------------------------------
// Allocators

void *BARR_gc_alloc_tracked(size_t size, const char *fn, const char *file, barr_i32 line)
{
    if (size == 0)
    {
        size = 1;
    }

    fn = fn ? fn : "N/A";
    file = file ? file : "N/A";

    void *p = malloc(size);

    if (p == NULL)
    {
        BARR_errlog("%s(): malloc failed (%zu bytes) at %s:%d", fn, size, file, line);
        return NULL;
    }

    barr_gc_push(p, size, fn, file, line);
    return p;
}

void *BARR_gc_calloc_tracked(size_t n, size_t size, const char *fn, const char *file, barr_i32 line)
{
    if (size == 0)
    {
        size = 1;
    }
    if (n == 0)
    {
        n = 1;
    }

    fn = fn ? fn : "N/A";
    file = file ? file : "N/A";

    void *p = calloc(n, size);

    if (p == NULL)
    {
        BARR_errlog("%s(): calloc failed (%zu bytes) at %s:%d", fn, n * size, file, line);
        return NULL;
    }

    barr_gc_push(p, n * size, fn, file, line);
    return p;
}

size_t barr_gc_get_size(void *ptr)
{
    if (ptr == NULL)
    {
        return 0;
    }
    pthread_mutex_lock(&g_barr_gc_list.lock);

    for (size_t i = g_barr_gc_list.count; i-- > 0;)
    {
        if (g_barr_gc_list.items[i].ptr == ptr)
        {
            size_t sz = g_barr_gc_list.items[i].size;
            pthread_mutex_unlock(&g_barr_gc_list.lock);
            return sz;
        }
    }
    pthread_mutex_unlock(&g_barr_gc_list.lock);
    return 0;
}

void *BARR_gc_realloc_tracked(void *ptr, size_t size, const char *fn, const char *file, barr_i32 line)
{
    fn = fn ? fn : "N/A";
    file = file ? file : "N/A";

    if (size == 0)
    {
        size = 1;
    }

    void *new_ptr = malloc(size);
    if (new_ptr == NULL)
    {
        BARR_errlog("%s(): malloc failed (%zu bytes) at %s:%d", fn, size, file, line);
        return NULL;
    }

    if (new_ptr)
    {
        size_t old_size = barr_gc_get_size(ptr);
        memcpy(new_ptr, ptr, old_size < size ? old_size : size);

        BARR_gc_free_tracked(ptr);
    }

    barr_gc_push(new_ptr, size, fn, file, line);
    return new_ptr;
}

char *BARR_gc_strdup_tracked(const char *src, const char *fn, const char *file, barr_i32 line)
{
    if (src == NULL)
    {
        BARR_errlog("%s(): NULL pointer passed to strdup", __func__);
        return NULL;
    }

    fn = fn ? fn : "N/A";
    file = file ? file : "N/A";

    size_t len = strlen(src) + 1;
    char *copy = BARR_gc_alloc_tracked(len, fn, file, line);

    if (copy == NULL)
    {
        return NULL;
    }

    memcpy(copy, src, len);
    return copy;
}

void BARR_gc_free_tracked(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    barr_gc_pop(ptr);
    free(ptr);
}

// ----------------------------------------------------------------------------------------------------
// Collection

void BARR_gc_collect_all(void)
{
    pthread_mutex_lock(&g_barr_gc_list.lock);

    for (size_t i = g_barr_gc_list.count; i-- > 0;)
    {
        free(g_barr_gc_list.items[i].ptr);
    }

    g_barr_gc_list.count = 0;
    pthread_mutex_unlock(&g_barr_gc_list.lock);
}

void BARR_gc_dump(void)
{
    pthread_mutex_lock(&g_barr_gc_list.lock);

    if (!g_barr_gc_list.count)
    {
        BARR_printf("[BARR_GC]: No active allocations\n");
        pthread_mutex_unlock(&g_barr_gc_list.lock);
        return;
    }

    BARR_printf(
        "==================================== BARR_GC Active Allocations ====================================\n\n");
    for (size_t i = 0; i < g_barr_gc_list.count; ++i)
    {
        BARR_GC_Info *a = &g_barr_gc_list.items[i];
        BARR_printf("[%zu] %p (%zu bytes) from %s (%s:%d)\n", i, a->ptr, a->size, a->fn, a->file, a->line);
    }
    BARR_printf(
        "====================================================================================================\n");

    pthread_mutex_unlock(&g_barr_gc_list.lock);
}

void BARR_gc_file_dump(void)
{
    pthread_mutex_lock(&g_barr_gc_list.lock);

    const char *filename = "barr_gc_dump.txt";
    if (!g_barr_gc_list.count)
    {
        BARR_file_write(filename, "[BARR_GC]: No active allocations\n");
        pthread_mutex_unlock(&g_barr_gc_list.lock);
        return;
    }

    BARR_file_write(
        filename, "==================================== BARR_GC Allocations ====================================\n\n");
    size_t total_allocated = 0;
    for (size_t i = 0; i < g_barr_gc_list.count; ++i)
    {
        BARR_GC_Info *a = &g_barr_gc_list.items[i];
        BARR_file_append(filename, "[%zu] %p (%zu bytes) %s: %s:%d\n", i, a->ptr, a->size, a->fn, a->file, a->line);
        total_allocated += a->size;
    }
    BARR_file_append(
        filename,
        "====================================================================================================\n");
    char *metric_str;
    if (total_allocated > 1024 * 1024)
    {
        total_allocated /= 1024 * 1024;
        metric_str = "MB";
    }
    else if (total_allocated > 1024)
    {
        total_allocated /= 1024;
        metric_str = "KB";
    }
    else
    {
        metric_str = "B";
    }
    BARR_file_append(filename, "Total allocated: %zu%s", total_allocated, metric_str);

    pthread_mutex_unlock(&g_barr_gc_list.lock);
}

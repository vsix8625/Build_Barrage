#include "barr_src_list.h"
#include "barr_arena.h"
#include "barr_debug.h"
#include "barr_gc.h"
#include "barr_hashmap.h"
#include "barr_io.h"
#include "barr_xxhash.h"
#include <stdlib.h>
#include <string.h>

bool BARR_source_list_init(BARR_SourceList *list, size_t initial_file_cap)
{
    if (!list)
    {
        return false;
    }
    list->entries = BARR_gc_calloc(initial_file_cap, sizeof(char *));
    if (!list->entries)
    {
        size_t list_size = initial_file_cap * sizeof(char *);
        BARR_errlog("%s(): failed to allocate %zu bytes for source list entries", __func__, list_size);
        list->count = 0;
        list->capacity = 0;
        list->path_buffer = NULL;
        list->path_buffer_size = 0;
        list->path_buffer_used = 0;
        return false;
    }

    list->path_buffer_size = initial_file_cap * BARR_PATH_MAX;
    list->path_buffer = BARR_gc_alloc(list->path_buffer_size);
    if (!list->path_buffer)
    {
        BARR_errlog("%s(): failed to allocate %zu bytes for path buffer", __func__, list->path_buffer_size);
        list->entries = NULL;
        list->count = 0;
        list->capacity = 0;
        list->path_buffer_size = 0;
        list->path_buffer_used = 0;
        return false;
    }

    list->count = 0;
    list->capacity = initial_file_cap;
    list->path_buffer_used = 0;
    BARR_dbglog("%s(): List capacity = %zu, path_buffer = %zu", __func__, initial_file_cap, list->path_buffer_size);

    return true;
}

bool BARR_source_list_push(BARR_SourceList *list, const char *path)
{
    if (list == NULL || path == NULL)
    {
        return false;
    }

    size_t len = strlen(path) + 1;
    if (list->count >= list->capacity || list->path_buffer_used + len > list->path_buffer_size)
    {
        size_t new_capacity = list->capacity ? list->capacity * 2 : (list->path_buffer_size / BARR_PATH_MAX) + 64;
        size_t new_path_buffer_size = new_capacity * BARR_PATH_MAX;

        char *new_path_buffer = BARR_gc_realloc(list->path_buffer, new_path_buffer_size);
        if (new_path_buffer == NULL)
        {
            BARR_errlog("%s(): failed to realloc path buffer (%zu bytes)", __func__, new_path_buffer_size);
            return false;
        }

        // if buffer moved , rebase
        if (new_path_buffer != list->path_buffer)
        {
            for (size_t i = 0; i < list->count; ++i)
            {
                list->entries[i] = new_path_buffer + (list->entries[i] - list->path_buffer);
            }
        }
        list->path_buffer = new_path_buffer;
        list->path_buffer_size = new_path_buffer_size;

        // realloc entries arr
        char **new_entries = BARR_gc_realloc(list->entries, new_capacity * sizeof(char *));
        if (new_entries == NULL)
        {
            BARR_errlog("%s(): failed to realloc entries (cap=%zu)", __func__, new_capacity);
            return false;
        }
        list->entries = new_entries;
        list->capacity = new_capacity;

        BARR_dbglog("%s(): realloc to: %zu", __func__, new_capacity);
    }
    char *copy = list->path_buffer + list->path_buffer_used;
    memcpy(copy, path, len);
    list->entries[list->count++] = copy;
    list->path_buffer_used += len;

    return true;
}

//----------------------------------------------------------------------------------------------------
// Hash file job

static void barr_hash_file_job(void *arg)
{
    BARR_HashJobArg *job = (BARR_HashJobArg *) arg;

    barr_u8 file_hash[BARR_XXHASH_LEN];
    barr_u8 include_hash[BARR_XXHASH_LEN];
    barr_u8 final_hash[BARR_XXHASH_LEN];

    if (job == NULL)
    {
        BARR_errlog("%s(): job is NULL", __func__);
        return;
    }

    if (!BARR_hash_file_xxh3(job->file, file_hash))
    {
        BARR_errlog("%s(): failed to hash files", __func__);
        return;
    }

    if (!BARR_hash_includes_xxh3(job->headers, job->file, include_hash))
    {
        BARR_errlog("%s(): failed to hash includes", __func__);
        return;
    }

    if (!BARR_hashes_merge_xxh3(file_hash, include_hash, (barr_u8 *) job->flags_hash, final_hash))
    {
        BARR_errlog("%s(): failed to merge hashes", __func__);
        return;
    }

    if (!BARR_hashmap_insert_ts(job->map, job->file, final_hash))
    {
        BARR_errlog("%s(): failed to insert", __func__);
    }
}

// in hashmap.h
bool BARR_source_list_hash_mt(BARR_SourceList *sources, BARR_SourceList *headers, BARR_HashMap *map,
                              const char *flags_str, BARR_ThreadPool *pool)
{
    if (sources == NULL || map == NULL || pool == NULL || headers == NULL || flags_str == NULL)
    {
        BARR_errlog("%s(): passed NULL pointer", __func__);
        return false;
    }

    // pre-compute flags hash
    barr_u8 tmp_flags_hash[BARR_XXHASH_LEN];
    if (!BARR_hash_flags_xxh3(flags_str, tmp_flags_hash))
    {
        BARR_errlog("%s(): failed hash", __func__);
    }
    barr_u8 *flags_hash_gc = (barr_u8 *) BARR_gc_alloc(sizeof(tmp_flags_hash));
    if (flags_hash_gc == NULL)
    {
        BARR_errlog("%s(): failed to allocate flags_hash_gc", __func__);
        return false;
    }
    memcpy(flags_hash_gc, tmp_flags_hash, sizeof(tmp_flags_hash));

    for (size_t j = 0; j < headers->count; ++j)
    {
        headers->entries[j] = BARR_gc_strdup(headers->entries[j]);
    }

    for (size_t i = 0; i < sources->count; ++i)
    {
        const char *src_path = sources->entries[i];

        BARR_HashJobArg *job_arg = (BARR_HashJobArg *) BARR_gc_alloc(sizeof(BARR_HashJobArg));
        if (job_arg == NULL)
        {
            BARR_errlog("%s(): OOM allocating job struct in GC for index %zu", __func__, i);
            continue;
        }

        // duplicate the src_path into GC so worker can safely read the string
        const char *file_gc = BARR_gc_strdup(src_path);

        job_arg->file = (const char *) file_gc;
        job_arg->flags_hash = (const barr_u8 *) flags_hash_gc;
        job_arg->map = map;
        job_arg->headers = headers;

        if (!BARR_thread_pool_add(pool, barr_hash_file_job, job_arg))
        {
            BARR_errlog("%s(): failed to add job for %s", __func__, job_arg->file);
            // no free GC owns job and file copy.
            continue;
        }
    }
    BARR_thread_pool_wait(pool);

    /* NOTE:
     * We intentionally do NOT destroy GC allocations here. GC memory is tracked and
     * will live for program lifetime (or until your GC frees it). This guarantees
     * job_arg->file and job_arg->flags_hash remain valid for the workers.
     */

    return true;
}

//----------------------------------------------------------------------------------------------------

void BARR_destroy_source_list(BARR_SourceList *list)
{
    if (list == NULL)
    {
        return;
    }
    list->path_buffer = NULL;
    list->entries = NULL;
    list->count = 0;
    list->capacity = 0;
    list->path_buffer_size = 0;
    list->path_buffer_used = 0;
    BARR_dbglog("%s(): destroyed source list", __func__);
}

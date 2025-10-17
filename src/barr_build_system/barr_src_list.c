#include "barr_src_list.h"
#include "barr_arena.h"
#include "barr_debug.h"
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
    list->entries = calloc(initial_file_cap, sizeof(char *));
    if (!list->entries)
    {
        size_t list_size = initial_file_cap * sizeof(char *);
        BARR_errlog("%s(): failed to allocate %zu bytes for source list entries", __func__, list_size);
        list->count = 0;
        list->capacity = 0;
        return false;
    }

    list->count = 0;
    list->capacity = initial_file_cap;

    BARR_dbglog("%s(): cap=%zu", __func__, initial_file_cap);
    return true;
}

bool BARR_source_list_push(BARR_SourceList *list, const char *path)
{
    if (!list || !path)
    {
        return false;
    }

    if (list->count >= list->capacity)
    {
        size_t new_capacity = list->capacity * 2;
        char **new_entries = realloc(list->entries, new_capacity * sizeof(char *));
        if (!new_entries)
        {
            BARR_errlog("%s(): failed to realloc entries (cap=%zu)", __func__, new_capacity);
            return false;
        }

        list->entries = new_entries;
        list->capacity = new_capacity;

        BARR_dbglog("%s(): entries to grow: %zu", __func__, new_capacity);
    }

    char *copy = strdup(path);
    if (!copy)
    {
        return false;
    }

    list->entries[list->count++] = copy;
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

    if (!BARR_hash_file_xxh3(job->file, file_hash))
    {
        BARR_errlog("%s(): failed to hash file %s", __func__, job->file);
        return;
    }

    if (!BARR_hash_includes_xxh3(job->file, include_hash))
    {
        BARR_errlog("%s(): failed to hash includes for %s", __func__, job->file);
        return;
    }

    if (!BARR_hashes_merge_xxh3(file_hash, include_hash, (barr_u8 *) job->flags_hash, final_hash))
    {
        BARR_errlog("%s(): failed to merge hashes", __func__);
        return;
    }

    if (!BARR_hashmap_insert_ts(job->map, job->file, final_hash))
    {
        BARR_errlog("%s(): failed to insert %s", __func__, job->file);
    }
}

bool BARR_source_list_hash_mt(BARR_SourceList *list, BARR_HashMap *map, const char *flags_str, BARR_ThreadPool *pool)
{
    if (!list || !map || !pool)
    {
        return false;
    }

    // pre-compute flags hash
    barr_u8 flags_hash[BARR_XXHASH_LEN];
    if (!BARR_hash_flags_xxh3(flags_str, flags_hash))
    {
        BARR_errlog("%s(): failed hash %s", __func__, flags_str);
    }

    size_t arena_size = BARR_align_up(sizeof(BARR_HashJobArg), 16) * list->count;
    BARR_Arena arena = {0};
    BARR_arena_init(&arena, arena_size, "hash_job_arena", 16);

    for (size_t i = 0; i < list->count; ++i)
    {
        BARR_HashJobArg *job_arg = (BARR_HashJobArg *) BARR_arena_alloc(&arena, sizeof(BARR_HashJobArg));

        job_arg->file = list->entries[i];
        job_arg->flags_hash = flags_hash;
        job_arg->map = map;

        if (!BARR_thread_pool_add(pool, barr_hash_file_job, job_arg))
        {
            BARR_errlog("%s(): failed to add job for %s", __func__, list->entries[i]);
        }
    }

    BARR_thread_pool_wait(pool);
    BARR_destroy_arena(&arena);

    return true;
}

//----------------------------------------------------------------------------------------------------

bool BARR_destroy_source_list(BARR_SourceList *list)
{
    if (!list)
    {
        return false;
    }
    if (list->entries)
    {
        for (size_t i = 0; i < list->count; ++i)
        {
            if (list->entries[i])
            {
                free(list->entries[i]);
            }
        }
        free(list->entries);
        list->entries = NULL;
    }
    list->count = 0;
    list->capacity = 0;

    BARR_dbglog("%s(): destroyed source list", __func__);
    return true;
}

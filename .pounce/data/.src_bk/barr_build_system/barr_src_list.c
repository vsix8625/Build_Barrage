#include "barr_src_list.h"
#include "barr_debug.h"
#include "barr_io.h"
#include "barr_sha256.h"
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

bool BARR_source_list_hash(BARR_SourceList *list, BARR_HashMap *map, const char *flags_str)
{
    if (!list || !map)
    {
        return false;
    }

    for (size_t i = 0; i < list->count; i++)
    {
        const char *file = list->entries[i];
        barr_u8 file_hash[BARR_SHA256_LEN];
        barr_u8 include_hash[BARR_SHA256_LEN];
        barr_u8 flags_hash[BARR_SHA256_LEN];
        barr_u8 final_hash[BARR_SHA256_LEN];

        if (!BARR_hash_file_sha256(file, file_hash))
        {
            BARR_errlog("%s(): failed hash %s", __func__, file);
            continue;
        }
        if (!BARR_hash_includes_sha256(file, include_hash))
        {
            BARR_errlog("%s(): failed hash %s", __func__, file);
            continue;
        }
        // flags should be in source list
        if (!BARR_hash_flags_sha256(flags_str, flags_hash))
        {
            BARR_errlog("%s(): failed hash %s", __func__, flags_str);
            continue;
        }
        if (!BARR_hashes_merge_sha256(file_hash, include_hash, flags_hash, final_hash))
        {
            BARR_errlog("%s(): failed merge hashes", __func__);
            continue;
        }

        if (!BARR_hashmap_insert(map, file, final_hash))
        {
            return false;
        }
    }
    return true;
}

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

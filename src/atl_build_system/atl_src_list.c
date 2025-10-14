#include "atl_src_list.h"
#include "atl_debug.h"
#include "atl_io.h"
#include "atl_sha256.h"
#include <stdlib.h>
#include <string.h>

bool ATL_source_list_init(ATL_SourceList *list, size_t initial_file_cap)
{
    if (!list)
    {
        return false;
    }
    list->entries = calloc(initial_file_cap, sizeof(char *));
    if (!list->entries)
    {
        size_t list_size = initial_file_cap * sizeof(char *);
        ATL_errlog("%s(): failed to allocate %zu bytes for source list entries", __func__, list_size);
        list->count = 0;
        list->capacity = 0;
        return false;
    }

    list->count = 0;
    list->capacity = initial_file_cap;

    ATL_dbglog("%s(): cap=%zu", __func__, initial_file_cap);
    return true;
}

bool ATL_source_list_push(ATL_SourceList *list, const char *path)
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
            ATL_errlog("%s(): failed to realloc entries (cap=%zu)", __func__, new_capacity);
            return false;
        }

        list->entries = new_entries;
        list->capacity = new_capacity;

        ATL_dbglog("%s(): entries to grow: %zu", __func__, new_capacity);
    }

    char *copy = strdup(path);
    if (!copy)
    {
        return false;
    }

    list->entries[list->count++] = copy;
    return true;
}

bool ATL_source_list_hash(ATL_SourceList *list, ATL_HashMap *map, const char *flags_str)
{
    if (!list || !map)
    {
        return false;
    }

    for (size_t i = 0; i < list->count; i++)
    {
        const char *file = list->entries[i];
        atl_u8 file_hash[ATL_SHA256_LEN];
        atl_u8 include_hash[ATL_SHA256_LEN];
        atl_u8 flags_hash[ATL_SHA256_LEN];
        atl_u8 final_hash[ATL_SHA256_LEN];

        if (!ATL_hash_file_sha256(file, file_hash))
        {
            continue;
        }
        if (!ATL_hash_includes_sha256(file, include_hash))
        {
            continue;
        }
        // flags should be in source list
        if (!ATL_hash_flags_sha256(flags_str, flags_hash))
        {
            continue;
        }
        if (!ATL_hashes_merge_sha256(file_hash, include_hash, flags_hash, final_hash))
        {
            continue;
        }

        if (!ATL_hashmap_insert(map, file, final_hash))
        {
            return false;
        }
    }
    return true;
}

bool ATL_destroy_source_list(ATL_SourceList *list)
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

    ATL_dbglog("%s(): destroyed source list", __func__);
    return true;
}

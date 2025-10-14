#include "atl_hashmap.h"
#include "atl_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline atl_i64 atl_hash_str(const char *str)
{
    atl_u64 hash = 5381;
    atl_i32 c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

ATL_HashMap *ATL_hashmap_create(size_t capacity)
{
    ATL_HashMap *map = malloc(sizeof(ATL_HashMap));
    if (!map)
    {
        ATL_errlog("%s(): failed to allocate memory for hashmap", __func__);
        return NULL;
    }

    map->nodes = calloc(capacity, sizeof(ATL_HashNode *));
    if (!map->nodes)
    {
        free(map);
        ATL_errlog("%s(): failed to allocate memory for hashmap nodes", __func__);
        return NULL;
    }

    map->capacity = capacity;
    map->count = 0;
    return map;
}

bool ATL_destroy_hashmap(ATL_HashMap *map)
{
    if (!map)
    {
        return false;
    }

    for (size_t i = 0; i < map->capacity; ++i)
    {
        ATL_HashNode *node = map->nodes[i];
        while (node)
        {
            ATL_HashNode *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }

    free(map->nodes);
    free(map);
    return true;
}

bool atl_hashmap_rehash(ATL_HashMap *map, size_t new_capacity)
{
    if (!map || new_capacity <= map->capacity)
        return false;

    ATL_HashNode **new_nodes = calloc(new_capacity, sizeof(ATL_HashNode *));
    if (!new_nodes)
        return false;

    // re-insert all nodes
    for (size_t i = 0; i < map->capacity; ++i)
    {
        ATL_HashNode *node = map->nodes[i];
        while (node)
        {
            ATL_HashNode *next = node->next;
            unsigned long h = atl_hash_str(node->key) % new_capacity;
            node->next = new_nodes[h];
            new_nodes[h] = node;
            node = next;
        }
    }

    free(map->nodes);
    map->nodes = new_nodes;
    map->capacity = new_capacity;
    return true;
}

bool ATL_hashmap_insert(ATL_HashMap *map, const char *key, const atl_u8 hash[ATL_SHA256_LEN])
{
    if (!map || !key || !hash)
    {
        ATL_errlog("%s(): map key or hash are NULL", __func__);
        return false;
    }

    if ((double) map->count / map->capacity > 0.7)
    {
        if (!atl_hashmap_rehash(map, map->capacity * 2))
        {
            ATL_errlog("%s(): failed to realloc hashmap", __func__);
            return false;
        }
    }

    atl_u64 h = atl_hash_str(key) % map->capacity;
    ATL_HashNode *node = map->nodes[h];

    while (node)
    {
        if (ATL_strmatch(node->key, key))
        {
            memcpy(node->hash, hash, ATL_SHA256_LEN);
            return true;
        }
        node = node->next;
    }

    ATL_HashNode *new_node = malloc(sizeof(ATL_HashNode));
    if (!new_node)
    {
        ATL_errlog("%s(): failed to allocate memory for new_node", __func__);
        return false;
    }

    new_node->key = strdup(key);
    memcpy(new_node->hash, hash, ATL_SHA256_LEN);
    new_node->next = map->nodes[h];
    map->nodes[h] = new_node;
    map->count++;

    return true;
}

const atl_u8 *ATL_hashmap_get(ATL_HashMap *map, const char *key)
{
    if (!map || !key)
    {
        return NULL;
    }

    atl_u64 h = atl_hash_str(key) % map->capacity;
    ATL_HashNode *node = map->nodes[h];
    while (node)
    {
        if (strcmp(node->key, key) == 0)
        {
            return node->hash;
        }
        node = node->next;
    }
    return NULL;
}

void ATL_hashmap_debug(const ATL_HashMap *map)
{
    ATL_log("HashMap: %zu entries, %zu nodes", map->count, map->capacity);
    size_t max_chain = 0;
    for (size_t i = 0; i < map->capacity; ++i)
    {
        size_t chain = 0;
        for (ATL_HashNode *n = map->nodes[i]; n; n = n->next)
        {
            chain++;
        }
        if (chain > max_chain)
        {
            max_chain = chain;
        }
    }
    ATL_log("Longest collision chain: %zu", max_chain);
}

bool ATL_hashmap_write_cache(const ATL_HashMap *map, const char *cache_file)
{
    if (!map)
    {
        return false;
    }

    FILE *fp = fopen(cache_file, "w");
    if (!fp)
    {
        ATL_errlog("%s(): failed to open cache file %s", __func__, cache_file);
        return false;
    }

    for (size_t i = 0; i < map->capacity; ++i)
    {
        for (ATL_HashNode *node = map->nodes[i]; node; node = node->next)
        {
            fprintf(fp, "%s ", node->key);
            for (size_t j = 0; j < ATL_SHA256_LEN; ++j)
            {
                fprintf(fp, "%02x", node->hash[j]);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return true;
}

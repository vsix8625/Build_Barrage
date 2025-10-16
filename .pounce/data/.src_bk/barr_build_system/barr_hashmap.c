#include "barr_hashmap.h"
#include "barr_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline barr_i64 barr_hash_str(const char *str)
{
    barr_u64 hash = 5381;
    barr_i32 c;
    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

BARR_HashMap *BARR_hashmap_create(size_t capacity)
{
    BARR_HashMap *map = malloc(sizeof(BARR_HashMap));
    if (!map)
    {
        BARR_errlog("%s(): failed to allocate memory for hashmap", __func__);
        return NULL;
    }

    map->nodes = calloc(capacity, sizeof(BARR_HashNode *));
    if (!map->nodes)
    {
        free(map);
        BARR_errlog("%s(): failed to allocate memory for hashmap nodes", __func__);
        return NULL;
    }

    map->capacity = capacity;
    map->count = 0;
    return map;
}

bool BARR_destroy_hashmap(BARR_HashMap *map)
{
    if (!map)
    {
        return false;
    }

    for (size_t i = 0; i < map->capacity; ++i)
    {
        BARR_HashNode *node = map->nodes[i];
        while (node)
        {
            BARR_HashNode *next = node->next;
            free(node->key);
            free(node);
            node = next;
        }
    }

    free(map->nodes);
    free(map);
    return true;
}

bool barr_hashmap_rehash(BARR_HashMap *map, size_t new_capacity)
{
    if (!map || new_capacity <= map->capacity)
        return false;

    BARR_HashNode **new_nodes = calloc(new_capacity, sizeof(BARR_HashNode *));
    if (!new_nodes)
        return false;

    // re-insert all nodes
    for (size_t i = 0; i < map->capacity; ++i)
    {
        BARR_HashNode *node = map->nodes[i];
        while (node)
        {
            BARR_HashNode *next = node->next;
            unsigned long h = barr_hash_str(node->key) % new_capacity;
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

bool BARR_hashmap_insert(BARR_HashMap *map, const char *key, const barr_u8 hash[BARR_SHA256_LEN])
{
    if (!map || !key || !hash)
    {
        BARR_errlog("%s(): map key or hash are NULL", __func__);
        return false;
    }

    if ((double) map->count / map->capacity > 0.7)
    {
        if (!barr_hashmap_rehash(map, map->capacity * 2))
        {
            BARR_errlog("%s(): failed to realloc hashmap", __func__);
            return false;
        }
    }

    barr_u64 h = barr_hash_str(key) % map->capacity;
    BARR_HashNode *node = map->nodes[h];

    while (node)
    {
        if (BARR_strmatch(node->key, key))
        {
            memcpy(node->hash, hash, BARR_SHA256_LEN);
            return true;
        }
        node = node->next;
    }

    BARR_HashNode *new_node = malloc(sizeof(BARR_HashNode));
    if (!new_node)
    {
        BARR_errlog("%s(): failed to allocate memory for new_node", __func__);
        return false;
    }

    new_node->key = strdup(key);
    memcpy(new_node->hash, hash, BARR_SHA256_LEN);
    new_node->next = map->nodes[h];
    map->nodes[h] = new_node;
    map->count++;

    return true;
}

const barr_u8 *BARR_hashmap_get(BARR_HashMap *map, const char *key)
{
    if (!map || !key)
    {
        return NULL;
    }

    barr_u64 h = barr_hash_str(key) % map->capacity;
    BARR_HashNode *node = map->nodes[h];
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

void BARR_hashmap_debug(const BARR_HashMap *map)
{
    BARR_log("HashMap: %zu entries, %zu nodes", map->count, map->capacity);
    size_t max_chain = 0;
    for (size_t i = 0; i < map->capacity; ++i)
    {
        size_t chain = 0;
        for (BARR_HashNode *n = map->nodes[i]; n; n = n->next)
        {
            chain++;
        }
        if (chain > max_chain)
        {
            max_chain = chain;
        }
    }
    BARR_log("Longest collision chain: %zu", max_chain);
}

bool BARR_hashmap_write_cache(const BARR_HashMap *map, const char *cache_file)
{
    if (!map)
    {
        return false;
    }

    FILE *fp = fopen(cache_file, "w");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open cache file %s", __func__, cache_file);
        return false;
    }

    for (size_t i = 0; i < map->capacity; ++i)
    {
        for (BARR_HashNode *node = map->nodes[i]; node; node = node->next)
        {
            fprintf(fp, "%s ", node->key);
            for (size_t j = 0; j < BARR_SHA256_LEN; ++j)
            {
                fprintf(fp, "%02x", node->hash[j]);
            }
            fprintf(fp, "\n");
        }
    }
    fclose(fp);
    return true;
}

BARR_HashMap *BARR_hashmap_read_cache(const char *cache_file)
{
    FILE *fp = fopen(cache_file, "r");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open cache file %s", __func__, cache_file);
        return NULL;
    }

    BARR_HashMap *map = BARR_hashmap_create(BARR_BUF_SIZE_8192);
    if (!map)
    {
        fclose(fp);
        return NULL;
    }

    char line[BARR_PATH_MAX + BARR_SHA256_LEN * 2 + 8];
    while (fgets(line, sizeof(line), fp))
    {
        char path[BARR_PATH_MAX];
        char hash_hex[BARR_SHA256_LEN * 2 + 1];

        if (sscanf(line, "%s %s", path, hash_hex) != 2)
        {
            continue;
        }

        barr_u8 hash_bin[BARR_SHA256_LEN];
        for (barr_i32 i = 0; i < BARR_SHA256_LEN; i++)
        {
            (void) sscanf(&hash_hex[i * 2], "%2hhx", &hash_bin[i]);
        }

        BARR_hashmap_insert(map, path, hash_bin);
    }

    fclose(fp);
    return map;
}

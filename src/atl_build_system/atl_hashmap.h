#ifndef ATL_HASHMAP_H_
#define ATL_HASHMAP_H_

#include "atl_defs.h"

#define ATL_HASH_MAP_DEFAULT_CAP 65536
#define ATL_HASH_MAP_KEY_MAX ATL_BUF_SIZE_1024

typedef struct ATL_HashNode
{
    char *key;
    atl_u8 hash[ATL_SHA256_LEN];
    struct ATL_HashNode *next;
} ATL_HashNode;

typedef struct ATL_HashMap
{
    ATL_HashNode **nodes;
    size_t capacity;
    size_t count;
} ATL_HashMap;

ATL_HashMap *ATL_hashmap_create(size_t capacity);
bool ATL_destroy_hashmap(ATL_HashMap *map);
bool ATL_hashmap_insert(ATL_HashMap *map, const char *key, const atl_u8 hash[ATL_SHA256_LEN]);
const atl_u8 *ATL_hashmap_get(ATL_HashMap *map, const char *key);
bool ATL_hashmap_write_cache(const ATL_HashMap *map, const char *cache_file);
void ATL_hashmap_debug(const ATL_HashMap *map);

#endif  // ATL_HASHMAP_H_

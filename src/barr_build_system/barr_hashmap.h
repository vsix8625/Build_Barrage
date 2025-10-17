#ifndef BARR_HASHMAP_H_
#define BARR_HASHMAP_H_

#include "barr_defs.h"
#include <bits/pthreadtypes.h>

#define BARR_HASH_MAP_DEFAULT_CAP 65536
#define BARR_HASH_MAP_KEY_MAX BARR_BUF_SIZE_1024

typedef struct BARR_HashNode
{
    char *key;
    barr_u8 hash[BARR_XXHASH_LEN];
    struct BARR_HashNode *next;
} BARR_HashNode;

typedef struct BARR_HashMap
{
    BARR_HashNode **nodes;
    size_t capacity;
    size_t count;
    pthread_mutex_t lock;
} BARR_HashMap;

typedef struct BARR_HashJobArg
{
    const char *file;
    const barr_u8 *flags_hash;
    BARR_HashMap *map;
} BARR_HashJobArg;

BARR_HashMap *BARR_hashmap_create(size_t capacity);
bool BARR_destroy_hashmap(BARR_HashMap *map);
bool BARR_hashmap_insert_ts(BARR_HashMap *map, const char *key, const barr_u8 hash[BARR_XXHASH_LEN]);
bool BARR_hashmap_insert(BARR_HashMap *map, const char *key, const barr_u8 hash[BARR_XXHASH_LEN]);
const barr_u8 *BARR_hashmap_get(BARR_HashMap *map, const char *key);
bool BARR_hashmap_write_cache(const BARR_HashMap *map, const char *cache_file);
BARR_HashMap *BARR_hashmap_read_cache(const char *cache_file);
void BARR_hashmap_debug(const BARR_HashMap *map);

#endif  // BARR_HASHMAP_H_

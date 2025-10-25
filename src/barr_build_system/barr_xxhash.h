#ifndef BARR_SHA256_H_
#define BARR_SHA256_H_

#include "barr_defs.h"
#include "barr_src_list.h"

typedef struct BARR_Filestack
{
    char **files;
    size_t count;
    size_t capacity;
} BARR_Filestack;

typedef struct BARR_IncludeCacheEntry
{
    char filename[BARR_PATH_MAX];
    char fullpath[BARR_PATH_MAX];
    struct BARR_IncludeCacheEntry *next;
} BARR_IncludeCacheEntry;

bool BARR_hash_file_xxh3(const char *filepath, barr_u8 out_hash[BARR_XXHASH_LEN]);
bool BARR_hash_includes_xxh3(const BARR_SourceList *headers, const char *filapath, barr_u8 out_hash[BARR_XXHASH_LEN]);
bool BARR_hash_flags_xxh3(const char *flags, barr_u8 out_hash[BARR_XXHASH_LEN]);
bool BARR_hashes_merge_xxh3(barr_u8 file_hash[BARR_XXHASH_LEN], barr_u8 include_hash[BARR_XXHASH_LEN],
                            barr_u8 flags_hash[BARR_XXHASH_LEN], barr_u8 out_hash[BARR_XXHASH_LEN]);
#endif  // BARR_SHA256_H_

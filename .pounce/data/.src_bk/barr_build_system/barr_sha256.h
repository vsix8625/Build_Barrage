#ifndef BARR_SHA256_H_
#define BARR_SHA256_H_

#include "barr_defs.h"

bool BARR_hash_file_sha256(const char *filepath, barr_u8 out_hash[BARR_SHA256_LEN]);
bool BARR_hash_includes_sha256(const char *filepath, barr_u8 out_hash[BARR_SHA256_LEN]);
bool BARR_hash_flags_sha256(const char *flags, barr_u8 out_hash[BARR_SHA256_LEN]);
bool BARR_hashes_merge_sha256(barr_u8 file_hash[BARR_SHA256_LEN], barr_u8 include_hash[BARR_SHA256_LEN],
                             barr_u8 flags_hash[BARR_SHA256_LEN], barr_u8 out_hash[BARR_SHA256_LEN]);

#endif  // BARR_SHA256_H_

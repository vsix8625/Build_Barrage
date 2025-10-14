#ifndef ATL_SHA256_H_
#define ATL_SHA256_H_

#include "atl_defs.h"

bool ATL_hash_file_sha256(const char *filepath, atl_u8 out_hash[ATL_SHA256_LEN]);
bool ATL_hash_includes_sha256(const char *filepath, atl_u8 out_hash[ATL_SHA256_LEN]);
bool ATL_hash_flags_sha256(const char *flags, atl_u8 out_hash[ATL_SHA256_LEN]);
bool ATL_hashes_merge_sha256(atl_u8 file_hash[ATL_SHA256_LEN], atl_u8 include_hash[ATL_SHA256_LEN],
                             atl_u8 flags_hash[ATL_SHA256_LEN], atl_u8 out_hash[ATL_SHA256_LEN]);

#endif  // ATL_SHA256_H_

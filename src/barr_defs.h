#pragma GCC system_header
#ifndef BARR_DEFS_H_
#define BARR_DEFS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int8_t barr_i8;
typedef int16_t barr_i16;
typedef int32_t barr_i32;
typedef int64_t barr_i64;

typedef uint8_t barr_u8;
typedef uint16_t barr_u16;
typedef uint32_t barr_u32;
typedef uint64_t barr_u64;

typedef float barr_f32;
typedef double barr_f64;

typedef void *barr_ptr;
typedef void (*barr_func_ptr)(void);

// BUFFER SIZES
#define BARR_BUF_SIZE_32 0x20
#define BARR_BUF_SIZE_64 0x40
#define BARR_BUF_SIZE_128 0x80
#define BARR_BUF_SIZE_256 0x100
#define BARR_BUF_SIZE_512 0x200
#define BARR_BUF_SIZE_1024 0x400
#define BARR_BUF_SIZE_2048 0x800
#define BARR_BUF_SIZE_4096 0x1000
#define BARR_BUF_SIZE_8192 0x2000

#define BARR_PATH_MAX (0x1000)

#define BARR_VOID(a) (void) (a)

#define BARR_LOG_VA_CHECK(fmtargnumber) __attribute__((format(__printf__, fmtargnumber, fmtargnumber + 1)))

// barr
#define BARR_MARKER_DIR ".barr"
#define BARR_MARKER_DATA_DIR "data"
#define BARR_MARKER_CACHE_DIR "cache"

#define BARR_NFTW_SOFT_CAP (512)
#define BARR_SCAN_MAX_DIRS (256)
#define BARR_SOURCE_LIST_INITIAL_FILES (0x1000)
#define BARR_AVG_PATH_LEN (256)

// cache
#define BARR_L1_CHUNK (16 * 1024)
#define BARR_L2_CHUNK (256 * 1024)

#define BARR_CACHE_DIR ".barr/cache"
#define BARR_CACHE_FILE ".barr/cache/build.cache"

#define BARR_XXHASH_LEN (8)

#endif  // BARR_DEFS_H_

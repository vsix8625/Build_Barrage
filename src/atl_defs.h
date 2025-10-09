#pragma GCC system_header
#ifndef ATL_DEFS_H_
#define ATL_DEFS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int8_t atl_i8;
typedef int16_t atl_i16;
typedef int32_t atl_i32;
typedef int64_t atl_i64;

typedef uint8_t atl_u8;
typedef uint16_t atl_u16;
typedef uint32_t atl_u32;
typedef uint64_t atl_u64;

typedef float atl_f32;
typedef double atl_f64;

typedef void *atl_ptr;
typedef void (*atl_func_ptr)(void);

// BUFFER SIZES
#define ATL_BUF_SIZE_32 0x20
#define ATL_BUF_SIZE_64 0x40
#define ATL_BUF_SIZE_128 0x80
#define ATL_BUF_SIZE_256 0x100
#define ATL_BUF_SIZE_512 0x200
#define ATL_BUF_SIZE_1024 0x400
#define ATL_BUF_SIZE_2048 0x800
#define ATL_BUF_SIZE_4096 0x1000
#define ATL_BUF_SIZE_8192 0x2000

#define ATL_PATH_MAX (0x1000)

#define ATL_VOID(a) (void) (a)

#define ATL_LOG_VA_CHECK(fmtargnumber) __attribute__((format(__printf__, fmtargnumber, fmtargnumber + 1)))

// atl
#define ATL_MARKER_DIR ".atl"
#define ATL_MARKER_DATA_DIR "data"

#define ATL_NFTW_SOFT_CAP (512)
#define ATL_SCAN_MAX_DIRS (256)
#define ATL_SOURCE_LIST_INITIAL_FILES (4096)
#define ATL_AVG_PATH_LEN (256)

#endif  // ATL_DEFS_H_

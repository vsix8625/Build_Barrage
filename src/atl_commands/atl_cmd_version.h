#ifndef ATL_CMD_VERSION_H_
#define ATL_CMD_VERSION_H_

#include "atl_defs.h"

#define ATL_VERSION_MAJOR 0
#define ATL_VERSION_MINOR 7
#define ATL_VERSION_PATCH 2

#define ATL_VERSION_DATE "2025-10-09"

#define ATL_VERSION_ENCODE(maj, min, pat) (((maj) << 22) | (min) << 12 | (pat))

#define ATL_VERSION_MAJOR_DECODE(v) (((v) >> 22) & 0x3FF)
#define ATL_VERSION_MINOR_DECODE(v) (((v) >> 12) & 0x3FF)
#define ATL_VERSION_PATCH_DECODE(v) ((v) & 0xFFF)

typedef enum
{
    ATL_INIT_NOT_FOUND = -1,
    ATL_INIT_LOCK_NEWER = -2,
    ATL_INIT_LOCK_OLDER = 0,
    ATL_INIT_OK = 1,
} ATL_InitStatus;

atl_i32 ATL_command_version(atl_i32 argc, char **argv);
atl_i32 ATL_read_version_code(const char *init_lock_file);
ATL_InitStatus ATL_check_initialized(void);
bool ATL_is_initialized(void);

#endif  // ATL_CMD_VERSION_H_

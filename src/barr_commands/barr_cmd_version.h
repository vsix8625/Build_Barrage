#ifndef BARR_CMD_VERSION_H_
#define BARR_CMD_VERSION_H_

#include "barr_defs.h"

#define BARR_VERSION_MAJOR 0
#define BARR_VERSION_MINOR 14
#define BARR_VERSION_PATCH 1

#define BARR_VERSION_DATE "2025-10-26"

#define BARR_VERSION_ENCODE(maj, min, pat) (((maj) << 22) | (min) << 12 | (pat))

#define BARR_VERSION_MAJOR_DECODE(v) (((v) >> 22) & 0x3FF)
#define BARR_VERSION_MINOR_DECODE(v) (((v) >> 12) & 0x3FF)
#define BARR_VERSION_PATCH_DECODE(v) ((v) & 0xFFF)

typedef enum
{
    BARR_INIT_NOT_FOUND = -1,
    BARR_INIT_LOCK_NEWER = -2,
    BARR_INIT_LOCK_OLDER = 0,
    BARR_INIT_OK = 1,
} BARR_InitStatus;

barr_i32 BARR_command_version(barr_i32 argc, char **argv);
barr_i32 BARR_read_version_code(const char *init_lock_file);
BARR_InitStatus BARR_check_initialized(void);
bool BARR_is_initialized(void);

#endif  // BARR_CMD_VERSION_H_

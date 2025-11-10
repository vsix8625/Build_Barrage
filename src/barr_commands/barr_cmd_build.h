#ifndef BARR_CMD_BUILD_H_
#define BARR_CMD_BUILD_H_

#include "barr_defs.h"

#define BARR_DATA_BUILD_INFO_PATH ".barr/data/build_info"
#define BARR_MAX_MODULES 128

barr_i32 BARR_command_build(barr_i32 argc, char **argv);

typedef struct
{
    char *name;
    char *path;
} BARR_Module;

bool BARR_add_module(const char *name, const char *path, const char *required);
BARR_Module *BARR_get_module(const char *name);
void BARR_print_modules(void);

#endif  // BARR_CMD_BUILD_H_

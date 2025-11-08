#ifndef BARR_CMD_BUILD_H_
#define BARR_CMD_BUILD_H_

#include "barr_defs.h"

bool BARR_add_module(const char *name, const char *path, const char *required);
barr_i32 BARR_command_build(barr_i32 argc, char **argv);

#endif  // BARR_CMD_BUILD_H_

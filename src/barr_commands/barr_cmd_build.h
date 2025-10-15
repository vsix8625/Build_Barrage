#ifndef BARR_CMD_BUILD_H_
#define BARR_CMD_BUILD_H_

#include "barr_defs.h"

typedef struct BARR_CompileCtx
{
    const char *compiler;
    char **cflags;
    size_t cflags_count;
    const char *out_dir;
    bool verbose;
} BARR_CompileCtx;  // TODO: make an initializer for the ctx and cleanup

barr_i32 BARR_command_build(barr_i32 argc, char **argv);

#endif  // BARR_CMD_BUILD_H_

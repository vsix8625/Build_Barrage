#ifndef ATL_COMMANDS_H_
#define ATL_COMMANDS_H_

#include "atl_defs.h"

#define ATL_MAX_COMMANDS 64

typedef atl_i32 (*atl_cmd_fn)(atl_i32 argc, char **argv);

typedef struct ATL_Command
{
    const char *name;
    atl_cmd_fn fn;

    const char *help;
    const char **aliases;  // null terminated list
} ATL_Command;

// include all commands
#include "atl_cmd_help.h"
#include "atl_cmd_version.h"

#endif  // ATL_COMMANDS_H_

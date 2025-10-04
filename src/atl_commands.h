#ifndef ATL_COMMANDS_H_
#define ATL_COMMANDS_H_

#include "atl_defs.h"
#include "atl_os_layer.h"

#define ATL_MAX_COMMANDS 64

typedef atl_i32 (*atl_cmd_fn)(atl_i32 argc, char **argv);

typedef struct ATL_Command
{
    const char *name;
    atl_cmd_fn fn;

    const char *help;
    const char **aliases;  // null terminated list
    const char *detailed;
} ATL_Command;

// include all commands
#include "atl_cmd_build.h"
#include "atl_cmd_init.h"
#include "atl_cmd_new.h"
#include "atl_cmd_status.h"
#include "atl_cmd_version.h"

#if defined(ATL_OS_LINUX)
#include "atl_cmd_config_linux.h"
#elif defined(ATL_OS_WIN32)
// include cmd_config_win32
#endif

#endif  // ATL_COMMANDS_H_

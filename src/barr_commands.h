#ifndef BARR_COMMANDS_H_
#define BARR_COMMANDS_H_

#include "barr_defs.h"
#include "barr_os_layer.h"

#define BARR_MAX_COMMANDS 64

typedef barr_i32 (*barr_cmd_fn)(barr_i32 argc, char **argv);

typedef struct BARR_Command
{
    const char *name;
    barr_cmd_fn fn;

    const char *help;
    const char **aliases;  // null terminated list
    const char *detailed;
} BARR_Command;

// include all commands
#include "barr_cmd_build.h"
#include "barr_cmd_clean.h"
#include "barr_cmd_init.h"
#include "barr_cmd_mode.h"
#include "barr_cmd_new.h"
#include "barr_cmd_rebuild.h"
#include "barr_cmd_run.h"
#include "barr_cmd_status.h"
#include "barr_cmd_test.h"
#include "barr_cmd_tool.h"
#include "barr_cmd_version.h"

#if defined(BARR_OS_LINUX)
#include "barr_cmd_config_linux.h"
#elif defined(BARR_OS_WIN32)
// include cmd_config_win32
#endif

#endif  // BARR_COMMANDS_H_

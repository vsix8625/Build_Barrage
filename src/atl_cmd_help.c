#include "atl_cmd_help.h"
#include "atl_io.h"

atl_i32 ATL_command_help(atl_i32 argc, char **argv)
{
    ATL_log("Hello for help command");
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

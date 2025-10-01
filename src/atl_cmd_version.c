#include "atl_cmd_version.h"
#include "atl_io.h"

atl_i32 ATL_command_version(atl_i32 argc, char **argv)
{
    ATL_log("Hello for version command");
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

#include "atl_cmd_version.h"
#include "atl_io.h"

atl_i32 ATL_command_version(atl_i32 argc, char **argv)
{
    ATL_log("Atlas Build Manager (atl): v%d.%d.%d", ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);
    ATL_log("Date: %s", ATL_VERSION_DATE);

    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

#include "atl_cmd_new.h"
#include "atl_io.h"

atl_i32 ATL_command_new(atl_i32 argc, char **argv)
{
    if (argc < 2)
    {
        ATL_errlog("new requires option starting with '--'");
        ATL_errlog("Usage: atl new <opt> <arg>");
        return 1;
    }

    for (atl_i32 i = 1; i < argc; i++)
    {
        char *arg = argv[i];

        if (ATL_strmatch(arg, "--project") || ATL_strmatch(arg, "-p"))
        {
            if (i + 1 >= argc)
            {
                ATL_errlog("new --project requires <project_name>");
                return 1;
            }
            ATL_log("Create new project %s", argv[i + 1]);
            i++;
        }
        else if (ATL_strmatch(arg, "--file"))
        {
            if (i + 1 >= argc)
            {
                ATL_errlog("new --file requires <file_name>");
                return 1;
            }
            ATL_log("Create new file %s", argv[i + 1]);
            i++;
        }
        else
        {
            ATL_errlog("Unknown option: %s", arg);
            return 1;
        }
    }

    return 0;
}

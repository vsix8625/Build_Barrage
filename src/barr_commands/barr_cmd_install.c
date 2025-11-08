#include "barr_cmd_install.h"
#include "barr_cmd_version.h"
#include "barr_io.h"

static const char *barr_install_target(const char *target)
{
    if (target == NULL)
    {
        return NULL;
    }
    return NULL;
}

// NOTE: WIP
barr_i32 BARR_command_install(barr_i32 argc, char **argv)
{
    if (!BARR_is_initialized())
    {
        return 1;
    }

    char *prefix = NULL;
    char *destdir = NULL;

    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--prefix"))
        {
            if (i + 1 >= argc)
            {
                BARR_errlog("Option --prefix requires <dir_path>");
                return 1;
            }
            prefix = argv[i + 1];
            BARR_log("Prefix path set to: %s", prefix);
            i++;
        }
        else if (BARR_strmatch(cmd, "--destdir"))
        {
            if (i + 1 >= argc)
            {
                BARR_errlog("Option --destdir requires <dir_path>");
                return 1;
            }
            destdir = argv[i + 1];
            BARR_log("Destdir path set to: %s", destdir);
            i++;
        }
        else
        {
            BARR_warnlog("Invalid option for install command: %s", cmd);
            return 1;
        }
    }

    barr_install_target(NULL);

    BARR_log("Install completed!");
    return 0;
}

#include "barr_cmd_clean.h"
#include "barr_env.h"
#include "barr_io.h"
#include <stdio.h>

barr_i32 BARR_command_clean(barr_i32 argc, char **argv)
{
    if (!BARR_is_initialized())
    {
        return 1;
    }

    bool no_confirm = false;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--no-confirm") || BARR_strmatch(cmd, "-nc"))
        {
            no_confirm = true;
        }
        else
        {
            BARR_warnlog("Invalid option for clean command: %s", cmd);
        }
    }

    if (!no_confirm && !g_barr_silent_logs)
    {
        BARR_printf("Clean \"%s\"? [y/N]: ", BARR_getcwd());
        barr_i32 c = getchar();
        if (c != 'y' && c != 'Y')
        {
            BARR_warnlog("Clean aborted!");
            return 0;
        }
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
    }

    if (BARR_isdir("build"))
    {
        BARR_rmrf("build");
        if (BARR_isfile(BARR_CACHE_FILE))
        {
            BARR_rmrf(BARR_CACHE_FILE);
        }
    }

    BARR_log("Clean completed!");
    return 0;
}

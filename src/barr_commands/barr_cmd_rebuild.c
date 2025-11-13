#include "barr_cmd_rebuild.h"
#include "barr_cmd_build.h"
#include "barr_cmd_clean.h"
#include "barr_io.h"
#include <string.h>

barr_i32 BARR_command_rebuild(barr_i32 argc, char **argv)
{
    char *clean_argv[2];
    clean_argv[0] = "clean";
    clean_argv[1] = "-nc";
    if (BARR_command_clean(2, clean_argv))
    {
        BARR_warnlog("BARR_command_clean() abnormal exit");
    }

    char *filtered_argv[argc];
    barr_i32 filtered_count = 0;

    filtered_argv[filtered_count++] = argv[0];

    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];

        // Allow only safe flags
        if (BARR_strmatch(cmd, "--turbo") || BARR_strmatch(cmd, "--dry-run") || strncmp(cmd, "-j", 2) == 0 ||
            BARR_strmatch(cmd, "--threads"))
        {
            filtered_argv[filtered_count++] = cmd;

            // if --threads, include its argument
            if (BARR_strmatch(cmd, "--threads") && (i + 1 < argc))
            {
                filtered_argv[filtered_count++] = argv[++i];
            }
        }
        else
        {
            BARR_log("Ignoring unsafe flag in rebuild: %s", cmd);
        }
    }

    if (BARR_command_build(filtered_count, filtered_argv))
    {
        BARR_warnlog("BARR_command_build() abnormal exit");
    }

    return 0;
}

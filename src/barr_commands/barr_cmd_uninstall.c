#include "barr_cmd_uninstall.h"
#include "barr_cmd_version.h"
#include "barr_io.h"
#include <stdio.h>
#include <string.h>

barr_i32 BARR_command_uninstall(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);

    if (!BARR_init())
    {
        return 1;
    }

    if (getuid() != 0)
    {
        BARR_errlog("[%s]:%s(): sudo required", __TIME__, __func__);
        return 1;
    }

    FILE *f = fopen(BARR_DATA_INSTALL_INFO_PATH, "r");
    if (f == NULL)
    {
        BARR_errlog("No install info found, nothing to uninstall");
        return 1;
    }

    bool success = true;
    char line[BARR_PATH_MAX];

    while (fgets(line, sizeof(line), f))
    {
        // trim newline
        line[strcspn(line, "\r\n")] = 0;

        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }

        if (BARR_isdir(line) || BARR_isfile(line))
        {
            BARR_log("Removing: %s", line);
            if (BARR_rmrf(line) != 0)
            {
                BARR_warnlog("Failed to remove: %s", line);
                success = false;
            }
        }
        else
        {
            BARR_warnlog("Path does not exist, skipping: %s", line);
        }
    }

    if (success)
    {
        if (remove(BARR_DATA_INSTALL_INFO_PATH) == 0)
        {
            BARR_log("Removed install info");
        }
        else
        {
            BARR_warnlog("Failed to remove %s file", BARR_DATA_INSTALL_INFO_PATH);
        }
    }
    else
    {
        BARR_warnlog("Some files could not be removed. Install info kept for manual cleanup.");
    }

    fclose(f);
    return 0;
}

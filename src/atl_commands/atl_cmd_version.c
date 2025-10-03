#include "atl_cmd_version.h"
#include "atl_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

atl_i32 ATL_command_version(atl_i32 argc, char **argv)
{
    ATL_log("Atlas Build Manager (atl): v%d.%d.%d", ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);
    ATL_log("Date: %s", ATL_VERSION_DATE);

    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

atl_i32 atl_read_version_code(const char *init_lock_file)
{
    FILE *f = fopen(init_lock_file, "r");
    if (!f)
    {
        ATL_errlog("Failed to read %s", init_lock_file);
        return -1;
    }

    char line[ATL_BUF_SIZE_256];
    atl_i32 code = -1;

    while (fgets(line, sizeof(line), f))
    {
        if (strncmp(line, "version_code=", 13) == 0)
        {
            code = atoi(line + 13);
            break;
        }
    }

    fclose(f);
    return code;
}

bool ATL_is_initialized(void)
{
    char lockfile[ATL_BUF_SIZE_512];
    snprintf(lockfile, sizeof(lockfile), "%s/init.lock", ATL_MARKER_DIR);

    atl_i32 lock_version = atl_read_version_code(lockfile);
    atl_i32 current_version = ATL_VERSION_ENCODE(ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);

    if (lock_version < 0)
    {
        ATL_errlog("Seems like atl is not initialized in %s", ATL_getcwd());
        return false;
    }
    else if (lock_version > current_version)
    {
        ATL_errlog("This project was created with a newer version of atl. Please update!");
        ATL_errlog("Lock version: %d", lock_version);
        ATL_errlog("Current version: %d", current_version);
        return false;
    }
    else if (lock_version < current_version)
    {
        ATL_warnlog("This project is initialized with an older atl version (lock: %d, current: %d)", lock_version,
                    current_version);
        return true;
    }
    else
    {
        ATL_log("Version match: %d, initialized", lock_version);
        return true;
    }
}

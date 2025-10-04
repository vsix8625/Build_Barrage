#include "atl_cmd_version.h"
#include "atl_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

atl_i32 ATL_command_version(atl_i32 argc, char **argv)
{
    ATL_printf("Atlas Build Manager (atl): v%d.%d.%d\n", ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);
    ATL_printf("Date: %s\n", ATL_VERSION_DATE);

    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

atl_i32 ATL_read_version_code(const char *init_lock_file)
{
    FILE *f = fopen(init_lock_file, "r");
    if (!f)
    {
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

ATL_InitStatus ATL_check_initialized(void)
{
    char lockfile[ATL_BUF_SIZE_512];
    snprintf(lockfile, sizeof(lockfile), "%s/init.lock", ATL_MARKER_DIR);

    atl_i32 lock_version = ATL_read_version_code(lockfile);
    atl_i32 current_version = ATL_VERSION_ENCODE(ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);

    if (lock_version < 0)
    {
        return ATL_INIT_NOT_FOUND;
    }

    if (lock_version > current_version)
    {
        return ATL_INIT_LOCK_NEWER;
    }

    if (lock_version < current_version)
    {
        return ATL_INIT_LOCK_OLDER;
    }

    return ATL_INIT_OK;
}

bool ATL_is_initialized(void)
{
    ATL_InitStatus res = ATL_check_initialized();
    atl_i32 current_version = ATL_VERSION_ENCODE(ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH);

    switch (res)
    {
        case ATL_INIT_NOT_FOUND:
        {
            ATL_errlog("Atl is not initialized in %s", ATL_getcwd());
            ATL_log("Run 'atl init' to initialize this directory.");
            return false;
        }

        case ATL_INIT_LOCK_NEWER:
        {
            ATL_errlog("This project was created with a newer version of Atl. Please update!");
            ATL_errlog("Lock version: %d", ATL_read_version_code(".atl/init.lock"));
            ATL_errlog("Current version: %d", current_version);
            return false;
        }

        case ATL_INIT_LOCK_OLDER:
        {
            ATL_warnlog("Project initialized with an older Atl version (lock: %d, current: %d)",
                        ATL_read_version_code(".atl/init.lock"), current_version);
            return true;
        }

        case ATL_INIT_OK:
        {
            ATL_log("Atl version %d.%d.%d initialized in %s", ATL_VERSION_MAJOR, ATL_VERSION_MINOR, ATL_VERSION_PATCH,
                    ATL_getcwd());
            return true;
        }

        default:
        {
            ATL_errlog("Unknown initialization state!");
            return false;
        }
    }
}

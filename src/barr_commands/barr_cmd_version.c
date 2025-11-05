#include "barr_cmd_version.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *BARR_version_get_str(void)
{
    char buf[BARR_BUF_SIZE_32];
    snprintf(buf, sizeof(buf), "%d.%d.%d", BARR_VERSION_MAJOR, BARR_VERSION_MINOR, BARR_VERSION_PATCH);
    return BARR_gc_strdup(buf);
}

barr_i32 BARR_command_version(barr_i32 argc, char **argv)
{
    BARR_printf("Build Barrage (barr) version: %s-%s\n", BARR_version_get_str(), BARR_VERSION_BUILD_TYPE);
    BARR_printf("Latest update: %s\n", BARR_VERSION_DATE);

    BARR_VOID(argc);
    BARR_VOID(argv);
    return 0;
}

barr_i32 BARR_read_version_code(const char *init_lock_file)
{
    FILE *f = fopen(init_lock_file, "r");
    if (!f)
    {
        return -1;
    }

    char line[BARR_BUF_SIZE_256];
    barr_i32 code = -1;

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

BARR_InitStatus BARR_check_initialized(void)
{
    char lockfile[BARR_BUF_SIZE_512];
    snprintf(lockfile, sizeof(lockfile), "%s/init.lock", BARR_MARKER_DIR);

    barr_i32 lock_version = BARR_read_version_code(lockfile);
    barr_i32 current_version = BARR_VERSION_ENCODE(BARR_VERSION_MAJOR, BARR_VERSION_MINOR, BARR_VERSION_PATCH);

    if (lock_version < 0)
    {
        return BARR_INIT_NOT_FOUND;
    }

    if (lock_version > current_version)
    {
        return BARR_INIT_LOCK_NEWER;
    }

    if (lock_version < current_version)
    {
        return BARR_INIT_LOCK_OLDER;
    }

    return BARR_INIT_OK;
}

bool BARR_is_initialized(void)
{
    BARR_InitStatus res = BARR_check_initialized();
    barr_i32 current_version = BARR_VERSION_ENCODE(BARR_VERSION_MAJOR, BARR_VERSION_MINOR, BARR_VERSION_PATCH);

    switch (res)
    {
        case BARR_INIT_NOT_FOUND:
        {
            BARR_errlog("Build Barrage is not initialized in %s", BARR_getcwd());
            BARR_log("Run 'barr init' to initialize this directory.");
            return false;
        }

        case BARR_INIT_LOCK_NEWER:
        {
            BARR_errlog("This project was created with a newer version of Build Barrage. Please update!");
            BARR_errlog("Lock version: %d", BARR_read_version_code(".barr/init.lock"));
            BARR_errlog("Current version: %d", current_version);
            return false;
        }

        case BARR_INIT_LOCK_OLDER:
        {
            BARR_warnlog("Project initialized with an older Build barrage version (lock: %d, current: %d)",
                         BARR_read_version_code(".barr/init.lock"), current_version);
            return true;
        }

        case BARR_INIT_OK:
        {
            return true;
        }

        default:
        {
            BARR_errlog("Unknown initialization state!");
            return false;
        }
    }
}

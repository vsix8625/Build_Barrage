#include "atl_cmd_init.h"
#include "atl_io.h"
#include <stdio.h>
#include <string.h>

atl_i32 ATL_command_init(atl_i32 argc, char **argv)
{
    if (ATL_is_initialized())
    {
        ATL_errlog("It seems atl is already initialized in %s", ATL_getcwd());
        return 1;
    }

    char buffer[ATL_BUF_SIZE_1024];
    const char *cwd = ATL_getcwd();
    const char *last_slash = strrchr(cwd, ATL_PATH_SEPARATOR_CHAR);
    if (last_slash)
    {
        ATL_safecpy(buffer, last_slash + 1, sizeof(buffer));
    }
    else
    {
        ATL_safecpy(buffer, cwd, sizeof(buffer));
    }

    if (!ATL_isdir(ATL_MARKER_DIR))
    {
        if (atl_mkdir(ATL_MARKER_DIR))
        {
            ATL_errlog("Failed to create %s directory", ATL_MARKER_DIR);
            return 1;
        }

        atl_i32 major, minor, patch;
        major = ATL_VERSION_MAJOR;
        minor = ATL_VERSION_MINOR;
        patch = ATL_VERSION_PATCH;
        char init_lock_path[ATL_BUF_SIZE_2048];
        snprintf(init_lock_path, sizeof(init_lock_path), "%s/init.lock", ATL_MARKER_DIR);
        ATL_file_write(init_lock_path, "version=%d.%d.%d\nversion_code=%d\ndate=%s", major, minor, patch,
                       ATL_VERSION_ENCODE(major, minor, patch), ATL_VERSION_DATE);
        ATL_setperm(init_lock_path, "read-only");

        const char *atl_data_dir_path = ATL_MARKER_DIR ATL_PATH_SEPARATOR_STR ATL_MARKER_DATA_DIR;
        const char *dirs[] = {atl_data_dir_path};
        size_t count_dirs = sizeof(dirs) / sizeof(dirs[0]);

        for (size_t i = 0; i < count_dirs; ++i)
        {
            snprintf(buffer, sizeof(buffer), "%s", dirs[i]);
            if (atl_mkdir(buffer))
            {
                ATL_errlog("Failed to create %s directory", buffer);
                return 1;
            }
            ATL_dbglog("\t--> %zu: %s", i + 1, buffer);
        }
    }

    ATL_log("Initialized atl for %s", cwd);
    ATL_VOID(argc);
    ATL_VOID(argv);
    return 0;
}

atl_i32 ATL_command_deinit(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    if (ATL_is_initialized())
    {
        ATL_rmrf(ATL_MARKER_DIR);
        ATL_log("Uninitialized atl in %s directory", ATL_getcwd());
        return 0;
    }
    return 1;
}

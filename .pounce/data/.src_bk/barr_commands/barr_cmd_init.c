#include "barr_cmd_init.h"
#include "barr_debug.h"
#include "barr_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

barr_i32 BARR_command_init(barr_i32 argc, char **argv)
{
    const char *cwd = BARR_getcwd();
    if (BARR_isdir(BARR_MARKER_DIR))
    {
        BARR_log("It seems Build Barrage is already initialized in %s", cwd);
        return 1;
    }

    char buffer[BARR_BUF_SIZE_1024];
    const char *last_slash = strrchr(cwd, BARR_PATH_SEPARATOR_CHAR);
    if (last_slash)
    {
        BARR_safecpy(buffer, last_slash + 1, sizeof(buffer));
    }
    else
    {
        BARR_safecpy(buffer, cwd, sizeof(buffer));
    }

    const char *not_allowed_list[] = {BARR_GET_HOME(), "/", "/var", "/ect", "/usr", "/root", ".config"};
    size_t na_list_count = sizeof(not_allowed_list) / sizeof(not_allowed_list[0]);
    for (size_t i = 0; i < na_list_count; ++i)
    {
        if (BARR_strmatch(cwd, not_allowed_list[i]))
        {
            BARR_errlog("Cannot initialized to this directory: %s", cwd);
            return 1;
        }
    }

    if (!BARR_isdir("src") && !BARR_isfile("CMakeLists") && !BARR_isfile("build.barr") && !BARR_isfile("Makefile"))
    {
        BARR_warnlog("No project structure detected: %s", cwd);
    }

    // NOTE: it seems it never detects an empty dir
    if (BARR_isdir_empty(cwd))
    {
        BARR_warnlog("Initializing Build Barrage in an empty directory: %s", cwd);
    }

    if (!BARR_isdir(BARR_MARKER_DIR))
    {
        if (barr_mkdir(BARR_MARKER_DIR))
        {
            BARR_errlog("Failed to create %s directory", BARR_MARKER_DIR);
            return 1;
        }

        barr_i32 major, minor, patch;
        major = BARR_VERSION_MAJOR;
        minor = BARR_VERSION_MINOR;
        patch = BARR_VERSION_PATCH;
        char init_lock_path[BARR_BUF_SIZE_2048];
        snprintf(init_lock_path, sizeof(init_lock_path), "%s/init.lock", BARR_MARKER_DIR);
        BARR_file_write(init_lock_path, "version=%d.%d.%d\nversion_code=%d\ndate=%s", major, minor, patch,
                        BARR_VERSION_ENCODE(major, minor, patch), BARR_VERSION_DATE);
        BARR_setperm(init_lock_path, "read-only");

        const char *barr_data_dir_path = BARR_MARKER_DIR BARR_PATH_SEPARATOR_STR BARR_MARKER_DATA_DIR;
        const char *barr_cache_dir_path = BARR_MARKER_DIR BARR_PATH_SEPARATOR_STR BARR_MARKER_CACHE_DIR;
        const char *dirs[] = {barr_data_dir_path, barr_cache_dir_path};
        size_t count_dirs = sizeof(dirs) / sizeof(dirs[0]);

        for (size_t i = 0; i < count_dirs; ++i)
        {
            snprintf(buffer, sizeof(buffer), "%s", dirs[i]);
            if (barr_mkdir(buffer))
            {
                BARR_errlog("Failed to create %s directory", buffer);
                return 1;
            }
            BARR_dbglog("\t--> %zu: %s", i + 1, buffer);
        }
    }

    BARR_log("Initialized Build Barrage for %s", cwd);
    BARR_VOID(argc);
    BARR_VOID(argv);
    return 0;
}

barr_i32 BARR_command_deinit(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    if (BARR_is_initialized())
    {
        BARR_rmrf(BARR_MARKER_DIR);
        BARR_log("Uninitialized Build Barrage in %s directory", BARR_getcwd());
        return 0;
    }
    return 1;
}

#include "barr_cmd_new_create_project.h"
#include "barr_cmd_version.h"
#include "barr_debug.h"
#include "barr_io.h"
#include <stdio.h>

barr_i32 BARR_create_project(const char *project_name)
{
    barr_i32 mkdir_ret = barr_mkdir(project_name);
    if (mkdir_ret)
    {
        BARR_errlog("Failed to create %s", project_name);
        return 1;
    }
    BARR_dbglog("Created: %s directory", project_name);

    char buffer[BARR_BUF_SIZE_1024];
    snprintf(buffer, sizeof(buffer), "%s%s%s", project_name, BARR_PATH_SEPARATOR_STR, BARR_MARKER_DIR);
    BARR_dbglog("Generating %s sub directories", project_name);
    BARR_dbglog("\t--> 1: %s", buffer);

    if (barr_mkdir(buffer))
    {
        BARR_errlog("Failed to create %s directory", buffer);
        return 1;
    }

    barr_i32 major, minor, patch;
    major = BARR_VERSION_MAJOR;
    minor = BARR_VERSION_MINOR;
    patch = BARR_VERSION_PATCH;
    char init_lock_path[BARR_BUF_SIZE_2048];
    snprintf(init_lock_path, sizeof(init_lock_path), "%s%s%s", buffer, BARR_PATH_SEPARATOR_STR, "init.lock");
    BARR_file_write(init_lock_path, "version=%d.%d.%d\nversion_code=%d\ndate=%s", major, minor, patch,
                    BARR_VERSION_ENCODE(major, minor, patch), BARR_VERSION_DATE);
    BARR_setperm(init_lock_path, "read-only");

    char barr_marker_data_dir_buf[BARR_BUF_SIZE_1024];
    snprintf(barr_marker_data_dir_buf, sizeof(barr_marker_data_dir_buf), "%s%s%s%s", BARR_PATH_SEPARATOR_STR,
             BARR_MARKER_DIR, BARR_PATH_SEPARATOR_STR, BARR_MARKER_DATA_DIR);

    char barr_marker_cache_dir_buf[BARR_BUF_SIZE_1024];
    snprintf(barr_marker_cache_dir_buf, sizeof(barr_marker_cache_dir_buf), "%s%s%s%s", BARR_PATH_SEPARATOR_STR,
             BARR_MARKER_DIR, BARR_PATH_SEPARATOR_STR, BARR_MARKER_CACHE_DIR);

    const char *dirs[] = {barr_marker_data_dir_buf, barr_marker_cache_dir_buf, BARR_PATH_SEPARATOR_STR "src",
                          BARR_PATH_SEPARATOR_STR "inc"};
    size_t count_dirs = sizeof(dirs) / sizeof(dirs[0]);

    for (size_t i = 0; i < count_dirs; ++i)
    {
        snprintf(buffer, sizeof(buffer), "%s%s", project_name, dirs[i]);
        if (barr_mkdir(buffer))
        {
            BARR_errlog("Failed to create %s directory", buffer);
            return 1;
        }
        BARR_dbglog("\t--> %zu: %s", i + 2, buffer);
    }

    BARR_log("Successfully created: %s", project_name);
    BARR_log("You can now:");
    BARR_log("\tcd %s", project_name);
    BARR_log("\tbarr new --barrfile # Create a default project config");
    BARR_log("\tbarr config --local # Opens Barrfile with your $EDITOR");
    BARR_log("\tbarr new --main     # Create a minimal src/main.c");
    BARR_log("\tbarr build          # Auto build the project");
    BARR_log("\tbarr run            # Run the latest executable created\n");
    BARR_log("NOTE: you can also chain commands with `...` or '---'");
    BARR_log("\t(e.g): barr new --barrfile ... new --main ... build --turbo ... run");

    return 0;
}

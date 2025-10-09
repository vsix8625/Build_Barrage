#include "atl_cmd_new_create_project.h"
#include "atl_cmd_version.h"
#include "atl_debug.h"
#include "atl_io.h"
#include <stdio.h>

atl_i32 ATL_create_project(const char *project_name)
{
    atl_i32 mkdir_ret = atl_mkdir(project_name);
    if (mkdir_ret)
    {
        ATL_errlog("Failed to create %s", project_name);
        return 1;
    }
    ATL_dbglog("Created: %s directory", project_name);

    char buffer[ATL_BUF_SIZE_1024];
    snprintf(buffer, sizeof(buffer), "%s%s%s", project_name, ATL_PATH_SEPARATOR_STR, ATL_MARKER_DIR);
    ATL_dbglog("Generating %s sub directories", project_name);
    ATL_dbglog("\t--> 1: %s", buffer);

    if (atl_mkdir(buffer))
    {
        ATL_errlog("Failed to create %s directory", buffer);
        return 1;
    }

    atl_i32 major, minor, patch;
    major = ATL_VERSION_MAJOR;
    minor = ATL_VERSION_MINOR;
    patch = ATL_VERSION_PATCH;
    char init_lock_path[ATL_BUF_SIZE_2048];
    snprintf(init_lock_path, sizeof(init_lock_path), "%s%s%s", buffer, ATL_PATH_SEPARATOR_STR, "init.lock");
    ATL_file_write(init_lock_path, "version=%d.%d.%d\nversion_code=%d\ndate=%s", major, minor, patch,
                   ATL_VERSION_ENCODE(major, minor, patch), ATL_VERSION_DATE);
    ATL_setperm(init_lock_path, "read-only");

    char atl_marker_data_dir_buf[ATL_BUF_SIZE_1024];
    snprintf(atl_marker_data_dir_buf, sizeof(atl_marker_data_dir_buf), "%s%s%s%s", ATL_PATH_SEPARATOR_STR,
             ATL_MARKER_DIR, ATL_PATH_SEPARATOR_STR, ATL_MARKER_DATA_DIR);

    const char *dirs[] = {atl_marker_data_dir_buf, ATL_PATH_SEPARATOR_STR "src", ATL_PATH_SEPARATOR_STR "inc"};
    size_t count_dirs = sizeof(dirs) / sizeof(dirs[0]);

    for (size_t i = 0; i < count_dirs; ++i)
    {
        snprintf(buffer, sizeof(buffer), "%s%s", project_name, dirs[i]);
        if (atl_mkdir(buffer))
        {
            ATL_errlog("Failed to create %s directory", buffer);
            return 1;
        }
        ATL_dbglog("\t--> %zu: %s", i + 2, buffer);
    }

    ATL_log("Successfully created: %s", project_name);
    ATL_log("You can now:");
    ATL_log("\tcd %s", project_name);
    ATL_log("\tatl build");
    ATL_log("\tatl run");
    return 0;
}

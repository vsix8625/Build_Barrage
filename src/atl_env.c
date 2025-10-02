#include "atl_env.h"
#include "atl_io.h"
#include "atl_utils.h"
#include <stdio.h>
#include <stdlib.h>

static inline const char *atl_global_config_dir(void)
{
    static char path[ATL_BUF_SIZE_512];
    const char *base = ATL_GET_CONFIG_HOME();
    if (base)
    {
#if defined(ATL_OS_WIN32)
        snprintf(path, sizeof(path), "%s\\atlas_build_manager", base);
#elif defined(ATL_OS_LINUX)
        snprintf(path, sizeof(path), "%s/.config/atlas_build_manager", base);
#endif
    }
    return path[0] ? path : NULL;
}

static inline const char *atl_global_config_file(void)
{
    static char path[ATL_BUF_SIZE_1024];
    const char *dir = atl_global_config_dir();
    if (dir)
    {
#if defined(ATL_OS_WIN32)
        snprintf(path, sizeof(path), "%s\\atl.conf", dir);
#elif defined(ATL_OS_LINUX)
        snprintf(path, sizeof(path), "%s/atl.conf", dir);
#endif
    }
    return path[0] ? path : NULL;
}

#define ATL_GET_EDITOR() getenv("EDITOR")
static inline const char *atl_global_editor_env(void)
{
#if defined(ATL_OS_WIN32)
    const char *editor = ATL_GET_EDITOR() ? ATL_GET_EDITOR() : "notepad.exe";
#elif defined(ATL_OS_LINUX)
    const char *editor = ATL_GET_EDITOR() ? ATL_GET_EDITOR() : "vi";
#endif
    return editor;
}

//----------------------------------------------------------------------------------------------------

const char *ATL_get_config(const char *type)
{
    if (ATL_strmatch(type, "dir"))
    {
        return atl_global_config_dir();
    }
    if (ATL_strmatch(type, "file"))
    {
        return atl_global_config_file();
    }
    if (ATL_strmatch(type, "editor"))
    {
        return atl_global_editor_env();
    }
    return "Unknown";
}

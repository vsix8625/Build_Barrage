#include "barr_env.h"
#include "barr_io.h"
#include "barr_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>

static inline const char *barr_global_config_dir(void)
{
    static char path[BARR_BUF_SIZE_512];
    const char *base = BARR_GET_CONFIG_HOME();
    if (base)
    {
#if defined(BARR_OS_WIN32)
        snprintf(path, sizeof(path), "%s\\build_barrage", base);
#elif defined(BARR_OS_LINUX)
        snprintf(path, sizeof(path), "%s/.config/build_barrage", base);
#endif
    }
    return path[0] ? path : NULL;
}

static inline const char *barr_global_config_file(void)
{
    static char path[BARR_BUF_SIZE_1024];
    const char *dir = barr_global_config_dir();
    if (dir)
    {
#if defined(BARR_OS_WIN32)
        snprintf(path, sizeof(path), "%s\\barr.conf", dir);
#elif defined(BARR_OS_LINUX)
        snprintf(path, sizeof(path), "%s/barr.conf", dir);
#endif
    }
    return path[0] ? path : NULL;
}

#define BARR_GET_EDITOR() getenv("EDITOR")
static inline const char *barr_global_editor_env(void)
{
#if defined(BARR_OS_WIN32)
    const char *editor = BARR_GET_EDITOR() ? BARR_GET_EDITOR() : "notepad.exe";
#elif defined(BARR_OS_LINUX)
    const char *editor = BARR_GET_EDITOR() ? BARR_GET_EDITOR() : "vi";
#endif
    return editor;
}

//----------------------------------------------------------------------------------------------------
// Linux
#if defined(BARR_OS_LINUX)

static inline char *barr_lx_kernel_version(void)
{
    static struct utsname uts;
    if (uname(&uts) != 0)
    {
        return "barr_lx_unknown";
    }
    return uts.release;
}

static inline char *barr_lx_machine(void)
{
    static struct utsname uts;
    if (uname(&uts) != 0)
    {
        return "barr_lx_unknown";
    }
    return uts.machine;
}

#endif

//----------------------------------------------------------------------------------------------------

const char *BARR_get_config(const char *type)
{
    if (BARR_strmatch(type, "dir"))
    {
        return barr_global_config_dir();
    }
    if (BARR_strmatch(type, "file"))
    {
        return barr_global_config_file();
    }
    if (BARR_strmatch(type, "editor"))
    {
        return barr_global_editor_env();
    }
    if (BARR_strmatch(type, "version"))
    {
        return barr_lx_kernel_version();
    }
    if (BARR_strmatch(type, "machine"))
    {
        return barr_lx_machine();
    }
    return "Unknown";
}

#pragma GCC system_header
#ifndef ATL_UTILS_H_
#define ATL_UTILS_H_

#include "atl_cmd_version.h"
#include "atl_defs.h"
#include "atl_os_layer.h"

#define ATL_NULL_TERM_CHAR '\0'

static inline const char *ATL_getcwd(void)
{
    static char buf[ATL_BUF_SIZE_1024];
    return atl_getcwd(buf, sizeof(buf));
}

#define ATL_DEBUG_HELLO_FROM_FN() ATL_log("Hello from %s", __func__)
#define ATL_GET_VAR_NAME(var) (#var)

// Functions
bool ATL_strmatch(const char *s1, const char *s2);
void ATL_safecpy(char *dst, const char *src, size_t dst_size);
atl_i32 ATL_rmrf(const char *path);
bool ATL_mv(const char *src, const char *dst);
bool ATL_is_blank(const char *s);

bool ATL_is_modified(const char *path1, const char *path2);
bool ATL_is_src_newer(const char *src, const char *target);
bool ATL_isdir(const char *dir);
bool ATL_isfile(const char *path);
atl_i32 ATL_setperm(const char *path, const char *perm);

bool ATL_is_installed(const char *app);
bool ATL_isdir_empty(const char *path);

#endif  // ATL_UTILS_H_

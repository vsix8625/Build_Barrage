#pragma GCC system_header
#ifndef ATL_UTILS_H_
#define ATL_UTILS_H_

#include "atl_defs.h"
#include "atl_os_layer.h"

#define ATL_NULL_TERM_CHAR '\0'

static inline const char *ATL_getcwd(void)
{
    static char buf[ATL_BUF_SIZE_1024];
    return atl_getcwd(buf, sizeof(buf));
}

// Functions
bool ATL_strmatch(const char *s1, const char *s2);
void ATL_safecpy(char *dst, const char *src, size_t dst_size);
atl_i32 ATL_rmrf(const char *path);
bool ATL_mv(const char *src, const char *dst);

#endif  // ATL_UTILS_H_

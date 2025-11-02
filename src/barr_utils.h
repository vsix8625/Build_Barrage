#pragma GCC system_header
#ifndef BARR_UTILS_H_
#define BARR_UTILS_H_

#include "barr_cmd_version.h"
#include "barr_os_layer.h"

#define BARR_NULL_TERM_CHAR '\0'

static inline const char *BARR_getcwd(void)
{
    static char buf[BARR_BUF_SIZE_1024];
    return barr_getcwd(buf, sizeof(buf));
}

#define BARR_DEBUG_HELLO_FROM_FN() BARR_log("Hello from %s", __func__)
#define BARR_GET_VAR_NAME(var) (#var)

// Functions
bool BARR_strmatch(const char *s1, const char *s2);
void BARR_safecpy(char *dst, const char *src, size_t dst_size);
barr_i32 BARR_rmrf(const char *path);
bool BARR_mv(const char *src, const char *dst);
bool BARR_is_blank(const char *s);

bool BARR_is_modified(const char *path1, const char *path2);
bool BARR_is_src_newer(const char *src, const char *target);
bool BARR_isdir(const char *dir);
bool BARR_isfile(const char *path);
barr_i32 BARR_setperm(const char *path, const char *perm);

bool BARR_is_installed(const char *app);
char *BARR_which(const char *app);
bool BARR_isdir_empty(const char *path);

const char **BARR_dedup_flags_array(const char **src_arr);

char **BARR_tokenize_string(const char *str);

const char *BARR_fmt_time_elapsed(const struct timespec *start, const struct timespec *end);

barr_i32 BARR_mkdir_p(const char *path);

#endif  // BARR_UTILS_H_

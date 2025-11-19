#pragma GCC system_header
#ifndef BARR_UTILS_H_
#define BARR_UTILS_H_

#include "barr_cmd_version.h"
#include "barr_list.h"
#include "barr_math.h"
#include "barr_os_layer.h"

typedef enum
{
    BARR_SCAN_TYPE_ALL,
    BARR_SCAN_TYPE_DIR,
    BARR_SCAN_TYPE_FILE
} BARR_ScanType;

typedef struct
{
    char *path;
    size_t size;

    time_t modified_time;
    time_t accessed_time;

    nlink_t hard_links;
    mode_t mode;

    uid_t owner_id;
    gid_t group_id;
    dev_t device_id;

    blkcnt_t blocks_allocated;
} BARR_FSInfo;  // file system info

#define BARR_NULL_TERM_CHAR '\0'
#define BARRFILE_TIMESTAMP_PATH ".barr/data/Barrfile.stamp"

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
bool BARR_is_newer(const char *src, const char *target);
bool BARR_isdir(const char *dir);
bool BARR_isfile(const char *path);
barr_i32 BARR_setperm(const char *path, const char *perm);

char *BARR_find_in_path(const char *app);
bool BARR_is_installed(const char *app);
char *BARR_which(const char *app);
bool BARR_isdir_empty(const char *path);

const char **BARR_dedup_flags_array(const char **src_arr);

char **BARR_tokenize_string(const char *str);
size_t BARR_count_tokens_in_array(const char **arr);

const char *BARR_fmt_time_elapsed(const struct timespec *start, const struct timespec *end);

barr_i32 BARR_mkdir_p(const char *path);
bool BARR_is_valid_tool(const char *tool);

void BARR_update_Barrfile_stamp(void);
bool BARR_isdigit_str(const char *s);

void BARR_trim(char *s);
char *BARR_get_build_info_key(const char *file_path, const char *key);
void BARR_join_path(char *out, size_t out_size, const char *base, const char *rel);
bool BARR_is_absolute(const char *p);

bool BARR_path_resolve(const char *base, const char *rel, char *out, size_t out_size);
char *BARR_get_self_exe(void);

void BARR_object_files_scan(BARR_List *list, const char *dirpath);

void BARR_scan_dir(BARR_List *list, const char *dirpath, BARR_ScanType type);
void BARR_scan_dir_shallow(BARR_List *list, const char *dirpath, BARR_ScanType type);

void BARR_get_relative_path(char *out, size_t out_size, const char *root, const char *full_path);
void BARR_fsinfo_collect_stats_dir_r(BARR_List *list, const char *dirpath);
void BARR_list_fsinfo_print(const BARR_List *list);

const char *BARR_bytes_to_human(size_t bytes, char *out, size_t out_size);

#endif  // BARR_UTILS_H_

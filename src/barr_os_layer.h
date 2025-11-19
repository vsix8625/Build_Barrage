#pragma GCC system_header
#ifndef BARR_OS_LAYER_H_
#define BARR_OS_LAYER_H_

#include "barr_defs.h"

#include <sys/stat.h>

#include <linux/version.h>
#include <unistd.h>

#define BARR_OS_LINUX
#define BARR_OS_NAME "Linux"

#define BARR_GET_HOME() getenv("HOME")

#define BARR_OS_GET_CACHE_LINE_SIZE() sysconf(_SC_LEVEL1_DCACHE_LINESIZE)
#define BARR_OS_GET_CORES() sysconf(_SC_NPROCESSORS_ONLN)

#define barr_stat stat
typedef struct stat barr_stat_t;

#define barr_mkdir(dir) mkdir((dir), 0755)

#define BARR_DEVNULL "/dev/null"
#define BARR_PATH_SEPARATOR_STR "/"
#define BARR_PATH_SEPARATOR_CHAR '/'
#define BARR_PATH_DELIMITER ':'

#define barr_access access
#define barr_getcwd getcwd
#define BARR_SYSTEM_CMD_CLEAR "clear"

#define BARR_DYNAMIC_LIB_EXT "so"

barr_i32 BARR_run_process_BG(const char *name, char **args);
barr_i32 BARR_run_process(const char *name, char **args, bool verbose);
char *BARR_run_process_capture(char *const argv[]);
barr_i32 BARR_run_process_dev_null(const char *name, char **args);

#endif  // BARR_OS_LAYER_H_

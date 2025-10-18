#pragma GCC system_header
#ifndef BARR_OS_LAYER_H_
#define BARR_OS_LAYER_H_

#include "barr_defs.h"

#include <sys/stat.h>

#if defined(__linux)
#include <unistd.h>

#define BARR_OS_LINUX
#define BARR_OS_NAME "Linux"
#define BARR_GET_HOME() getenv("HOME")
#define BARR_GET_CONFIG_HOME() getenv("HOME")

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

barr_i32 BARR_run_process_BG(const char *name, char **args);
barr_i32 BARR_run_process(const char *name, char **args, bool verbose);

#include "barr_thread_pool.h"

#elif defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <io.h>

#define BARR_OS_WIN32
#define ALT_OS_NAME "Win32"
#define BARR_GET_HOME() getenv("USERPROFILE")
#define BARR_GET_CONFIG_HOME() getenv("APPDATA")

#define barr_stat _stat
typedef struct _stat barr_stat_t

#define barr_mkdir(dir) _mkdir((dir))

#define ALT_DEVNULL "nul"
#define BARR_PATH_SEPARATOR_STR "\\"
#define BARR_PATH_SEPARATOR_CHAR '\\'
#define BARR_PATH_DELIMITER ';'

#define barr_access _access
#define X_OK 0

#define barr_getcwd _getcwd
#define BARR_SYSTEM_CMD_CLEAR "cls"

#endif

#endif  // BARR_OS_LAYER_H_

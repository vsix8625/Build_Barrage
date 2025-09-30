#pragma GCC system_header
#ifndef ATL_OS_LAYER_H_
#define ATL_OS_LAYER_H_

#include <sys/stat.h>

#if defined(__linux)
#include <unistd.h>

#define ATL_OS_LINUX
#define ATL_OS_NAME "Linux"
#define ATL_GET_HOME() getenv("HOME")

#define alt_stat stat
typedef struct stat atl_stat_t;

#define atl_mkdir(dir) mkdir((dir), 0755)

#define ATL_DEVNULL "/dev/null"
#define ATL_PATH_SEPARATOR_STR "/"
#define ATL_PATH_SEPARATOR_CHAR '/'
#define ATL_PATH_DELIMITER ':'

#define atl_access access
#define atl_getcwd getcwd
#define ATL_SYSTEM_CMD_CLEAR "clear"

#elif defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <io.h>

#define ATL_OS_WIN32
#define ALT_OS_NAME "Win32"
#define ATL_GET_HOME() getenv("USERPROFILE")

#define atl_stat _stat
typedef struct _stat atl_stat_t

#define atl_mkdir(dir) _mkdir((dir))

#define ALT_DEVNULL "nul"
#define ATL_PATH_SEPARATOR_STR "\\"
#define ATL_PATH_SEPARATOR_CHAR '\\'
#define ATL_PATH_DELIMITER ';'

#define atl_access _access
#define X_OK 0

#define atl_getcwd _getcwd
#define ATL_SYSTEM_CMD_CLEAR "cls"

#endif

#endif  // ATL_OS_LAYER_H_

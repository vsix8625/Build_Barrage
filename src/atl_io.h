#ifndef ATL_IO_H_
#define ATL_IO_H_

#include "atl_utils.h"

void ATL_printf(const char *format, ...) ATL_LOG_VA_CHECK(1);
void ATL_log(const char *format, ...) ATL_LOG_VA_CHECK(1);
void ATL_warnlog(const char *format, ...) ATL_LOG_VA_CHECK(1);
void ATL_errlog(const char *format, ...) ATL_LOG_VA_CHECK(1);
void ATL_dbglog(const char *format, ...) ATL_LOG_VA_CHECK(1);

atl_i32 ATL_file_write(const char *filename, const char *format, ...) ATL_LOG_VA_CHECK(2);
atl_i32 ATL_file_append(const char *filename, const char *format, ...) ATL_LOG_VA_CHECK(2);

#endif  // ATL_IO_H_

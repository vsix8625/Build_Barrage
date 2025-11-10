#ifndef BARR_IO_H_
#define BARR_IO_H_

#include "barr_utils.h"

void BARR_printf(const char *format, ...) BARR_LOG_VA_CHECK(1);
void BARR_log(const char *format, ...) BARR_LOG_VA_CHECK(1);
void BARR_warnlog(const char *format, ...) BARR_LOG_VA_CHECK(1);
void BARR_errlog(const char *format, ...) BARR_LOG_VA_CHECK(1);

barr_i32 BARR_file_write(const char *filename, const char *format, ...) BARR_LOG_VA_CHECK(2);
barr_i32 BARR_file_append(const char *filename, const char *format, ...) BARR_LOG_VA_CHECK(2);

barr_i32 BARR_file_copy(const char *src, const char *dst);

barr_i32 BARR_fs_copy(const char *src, const char *dst);
barr_i32 BARR_fs_copy_tree(const char *src_dir, const char *dst_dir);

#endif  // BARR_IO_H_

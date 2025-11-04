#ifndef BARR_SRC_SCAN_H_
#define BARR_SRC_SCAN_H_

#include "barr_src_list.h"

#define BARR_SCAN_QUEUE_CAP (64)
#define BARR_SCAN_READAHEAD_SIZE (1024 * 1024 * 4)

void BARR_source_list_scan_dir(BARR_SourceList *list, const char *dirpath, const char **exclude_tokens);
void BARR_header_list_scan_dir(BARR_SourceList *list, const char *dirpath, BARR_SourceList *inc_dir_list,
                               const char **exclude_tokens);
#endif  // BARR_SRC_SCAN_H_

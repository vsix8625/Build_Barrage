#ifndef BARR_SRC_SCAN_H_
#define BARR_SRC_SCAN_H_

#include "barr_src_list.h"

#define BARR_SCAN_QUEUE_CAP (64)
#define BARR_SCAN_READAHEAD_SIZE (1024 * 1024 * 4)

void BARR_source_list_scan_dir(BARR_SourceList *list, const char *dirpath);

#endif  // BARR_SRC_SCAN_H_

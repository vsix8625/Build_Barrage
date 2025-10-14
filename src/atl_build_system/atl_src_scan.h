#ifndef ATL_SRC_SCAN_H_
#define ATL_SRC_SCAN_H_

#include "atl_src_list.h"

#define ATL_SCAN_QUEUE_CAP (64)
#define ATL_SCAN_READAHEAD_SIZE (1024 * 1024 * 4)

void ATL_source_list_scan_dir(ATL_SourceList *list, const char *dirpath);

#endif  // ATL_SRC_SCAN_H_

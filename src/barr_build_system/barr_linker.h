#ifndef BARR_LINKER_H_
#define BARR_LINKER_H_

#include "barr_defs.h"
#include "barr_src_list.h"

barr_i32 BARR_archive_stage(BARR_SourceList *list, const char *archive_path);
barr_i32 BARR_link_stage(char **link_args);

#endif  // BARR_LINKER_H_

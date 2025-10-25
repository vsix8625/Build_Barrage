#ifndef BARR_LINKER_H_
#define BARR_LINKER_H_

#include "barr_defs.h"
#include "barr_src_list.h"

#define BARR_INITIAL_LINK_ARGS_CAPACITY (16)

typedef struct BARR_LinkArgs
{
    char **args;
    barr_i32 count;
    barr_i32 capacity;
} BARR_LinkArgs;

barr_i32 BARR_archive_stage(BARR_SourceList *list, const char *archive_path);
barr_i32 BARR_link_stage(char **link_args);

BARR_LinkArgs *BARR_link_args_create(void);
void BARR_link_args_add(BARR_LinkArgs *la, const char *arg);
char **BARR_link_args_finalize(BARR_LinkArgs *la);

void BARR_dedup_libs_and_add_to_link_args(BARR_LinkArgs *la, const char **libs_array);

#endif  // BARR_LINKER_H_

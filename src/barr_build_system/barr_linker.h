#ifndef BARR_LINKER_H_
#define BARR_LINKER_H_

#include "barr_defs.h"
#include "barr_list.h"
#include "barr_src_list.h"

#define BARR_INITIAL_LINK_ARGS_CAPACITY (16)

typedef struct BARR_LinkArgs
{
    char **args;
    size_t count;
    size_t capacity;
} BARR_LinkArgs;

barr_i32 BARR_archive_stage(BARR_SourceList *list, const char *archive_path);
barr_i32 BARR_link_stage(char **link_args);

BARR_LinkArgs *BARR_link_args_create(void);
void BARR_link_args_add(BARR_LinkArgs *la, const char *arg);
char **BARR_link_args_finalize(BARR_LinkArgs *la);

barr_i32 BARR_link_target(const char *target_type, const char *target_name, const char *out_dir,
                          BARR_SourceList *object_list, BARR_List *pkg_list, size_t n_threads,
                          const char *resolved_compiler, const char *linker, const char *module_includes,
                          const char *version);

void BARR_link_collect_pkg_list(BARR_List *list, BARR_LinkArgs *input, BARR_LinkArgs *flags);

void BARR_dedup_libs_and_add_to_link_args(BARR_LinkArgs *la, const char **libs_array);

void BARR_link_args_debug(const BARR_LinkArgs *la);

#endif  // BARR_LINKER_H_

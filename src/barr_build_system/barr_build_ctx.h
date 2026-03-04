#ifndef BARR_BUILD_CTX_H_
#define BARR_BUILD_CTX_H_

#include "barr_defs.h"
#include "barr_list.h"
#include "barr_spin_lock.h"

#include <bits/pthreadtypes.h>

typedef struct BARR_BuildProgressCTX
{
    BARR_List *ccmds_json_entries_list;

    _Atomic size_t completed;
    _Atomic size_t failed;
    size_t         total;

    pthread_mutex_t log_mutex;
    barr_spinlock_t spin_lock;
} BARR_BuildProgressCTX;

typedef struct BARR_CompileCmdEntry
{
    char directory[BARR_PATH_MAX << 1];
    char file[BARR_PATH_MAX << 1];
    char command[BARR_BUF_SIZE_8192];
} BARR_CompileCmdEntry;

typedef struct BARR_CompileInfoCTX
{
    const char  *compiler;
    const char **flags;
    const char **includes;
    const char **defines;
    const char  *out_dir;

    bool debug_build;
    bool gen_compile_cmds;
    char ccmds_path[(BARR_PATH_MAX + 27) << 1];

    const char *pch_file;

    char pch_out[BARR_BUF_SIZE_2048];
} BARR_CompileInfoCTX;

typedef struct BARR_CompileJob
{
    BARR_CompileInfoCTX   *ctx;
    BARR_BuildProgressCTX *progress_ctx;

    char *src;

    char out_file[BARR_BUF_SIZE_8192 * 2];

    bool dry_run;
} BARR_CompileJob;

barr_i32 BARR_compile_pch(BARR_CompileInfoCTX *ctx);
void     BARR_compile_job(barr_ptr arg);

#endif  // BARR_BUILD_CTX_H_

#ifndef BARR_THREAD_JOBS_H_
#define BARR_THREAD_JOBS_H_

#include "barr_src_list.h"

typedef struct
{
    BARR_SourceList *sources;
    const char *filepath;
} BARR_WriteSourceArgs;

void BARR_write_sources_job(void *arg);

#endif  // BARR_THREAD_JOBS_H_

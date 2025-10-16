#ifndef BARR_THREAD_POOL_H_
#define BARR_THREAD_POOL_H_

#include "barr_defs.h"
#include <bits/pthreadtypes.h>

typedef void (*barr_jobfn)(void *);

typedef struct BARR_Job
{
    barr_jobfn fn;
    barr_ptr arg;
    struct BARR_Job *next;
} BARR_Job;

typedef struct BARR_ThreadPool
{
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pthread_cond_t empty;

    BARR_Job *jobs;
    barr_i32 stop;
    barr_i32 num_threads;
    pthread_t *threads;

    size_t pending;
    size_t active;
} BARR_ThreadPool;

BARR_ThreadPool *BARR_thread_pool_create(barr_i32 num_threads);
bool BARR_thread_pool_add(BARR_ThreadPool *p, barr_jobfn fn, barr_ptr arg);
bool BARR_thread_pool_wait(BARR_ThreadPool *p);
bool BARR_destroy_thread_pool(BARR_ThreadPool *p);

#endif  // BARR_THREAD_POOL_H_

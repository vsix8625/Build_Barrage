#include "barr_thread_pool.h"
#include "barr_io.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>

static barr_ptr barr_worker_loop(barr_ptr arg)
{
    BARR_ThreadPool *pool = (BARR_ThreadPool *) arg;
    BARR_Job *batch[BARR_JOB_BATCH_SIZE];

    for (;;)
    {
        size_t count = 0;

        pthread_mutex_lock(&pool->lock);

        while (!pool->jobs && !pool->stop)
        {
            pthread_cond_wait(&pool->cond, &pool->lock);
        }

        if (pool->stop)
        {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        while (pool->jobs && count < BARR_JOB_BATCH_SIZE)
        {
            batch[count++] = pool->jobs;
            pool->jobs = pool->jobs->next;
        }

        pool->active++;
        pthread_mutex_unlock(&pool->lock);

        for (size_t i = 0; i < count; ++i)
        {
            batch[i]->fn(batch[i]->arg);
            free(batch[i]);
        }

        pthread_mutex_lock(&pool->lock);

        pool->active--;
        if (!pool->jobs && !pool->active)
        {
            pthread_cond_signal(&pool->empty);
        }

        pthread_mutex_unlock(&pool->lock);
    }
    return NULL;
}

BARR_ThreadPool *BARR_thread_pool_create(barr_i32 num_threads)
{
    BARR_ThreadPool *p = calloc(1, sizeof(BARR_ThreadPool));
    if (p == NULL)
    {
        BARR_errlog("%s(): failed to allocate memory for thread pool", __func__);
        return NULL;
    }

    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->cond, NULL);
    pthread_cond_init(&p->empty, NULL);
    p->num_threads = num_threads;
    p->threads = calloc(num_threads, sizeof(pthread_t));
    if (!p->threads)
    {
        BARR_errlog("%s(): failed to allocate memory for pool threads", __func__);
        free(p);
        return NULL;
    }

    for (barr_i32 i = 0; i < num_threads; ++i)
    {
        if (!pthread_create(&p->threads[i], NULL, barr_worker_loop, p))
        {
            // pthread returns 0 on success
        }
    }

    return p;
}

bool BARR_thread_pool_add(BARR_ThreadPool *p, barr_jobfn fn, barr_ptr arg)
{
    if (p == NULL || arg == NULL)
    {
        return false;
    }

    BARR_Job *job = malloc(sizeof(BARR_Job));
    if (!job)
    {
        BARR_errlog("%s(): failed to allocate memory for job", __func__);
        return false;
    }

    job->fn = fn;
    job->arg = arg;
    job->next = NULL;

    pthread_mutex_lock(&p->lock);

    BARR_Job **tail = &p->jobs;
    while (*tail)
    {
        tail = &(*tail)->next;
    }
    *tail = job;

    pthread_mutex_unlock(&p->lock);
    pthread_cond_signal(&p->cond);

    return true;
}

bool BARR_thread_pool_wait(BARR_ThreadPool *p)
{
    if (p == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&p->lock);
    while (p->jobs || p->active > 0)
    {
        pthread_cond_wait(&p->empty, &p->lock);
    }
    pthread_mutex_unlock(&p->lock);

    return true;
}

bool BARR_destroy_thread_pool(BARR_ThreadPool *p)
{
    if (!p)
    {
        return false;
    }

    pthread_mutex_lock(&p->lock);
    p->stop = 1;
    pthread_cond_broadcast(&p->cond);
    pthread_mutex_unlock(&p->lock);

    for (barr_i32 i = 0; i < p->num_threads; ++i)
    {
        pthread_join(p->threads[i], NULL);
    }

    if (p->threads)
    {
        free(p->threads);
    }

    pthread_mutex_destroy(&p->lock);
    pthread_cond_destroy(&p->cond);
    pthread_cond_destroy(&p->empty);
    free(p);

    return true;
}

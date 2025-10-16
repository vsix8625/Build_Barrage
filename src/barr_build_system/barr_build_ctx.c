#include "barr_build_ctx.h"
#include "barr_debug.h"
#include "barr_io.h"

#include <assert.h>
#include <libgen.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// test it
barr_i32 BARR_compile_pch(BARR_CompileInfoCTX *ctx)
{
    if (!ctx->pch_file)
    {
        BARR_warnlog("No PCH detected");
        return 1;
    }

    char out_pch[BARR_BUF_SIZE_2048];
    snprintf(out_pch, sizeof(out_pch), "%s/pch_common.h.pch", ctx->out_dir);

    char *args[] = {(char *) ctx->compiler, "-x", "c-header", (char *) ctx->pch_file, "-o", out_pch, NULL};

    BARR_log("Compiling PCH: %s -> %s", ctx->pch_file, out_pch);

    int rc = BARR_run_process(args[0], args, false);
    if (rc == 0)
    {
        strncpy(ctx->pch_out, out_pch, sizeof(ctx->pch_out));
    }
    return rc;
}

void BARR_compile_job(barr_ptr arg)
{
    BARR_CompileJob *job = (BARR_CompileJob *) arg;
    const BARR_CompileInfoCTX *ctx = job->ctx;

    size_t flag_count = 0;
    size_t include_count = 0;
    size_t define_count = 0;
    if (ctx->flags)
    {
        for (const char **f = ctx->flags; *f; ++f)
        {
            flag_count++;
        }
    }
    if (ctx->includes)
    {
        for (const char **inc = ctx->includes; *inc; ++inc)
        {
            include_count++;
        }
    }
    if (ctx->defines)
    {
        for (const char **d = ctx->defines; *d; ++d)
        {
            define_count++;
        }
    }

    // +6 for (compiler -c src -o out_file NULL)
    size_t argc_total = flag_count + include_count + define_count + 9;
    char **args = calloc(argc_total, sizeof(char *));
    if (!args)
    {
        BARR_errlog("%s(): failed to allocate memory for args", __func__);
        free(job->src);
        free(job);
        return;
    }

    size_t idx = 0;
    args[idx++] = (char *) ctx->compiler;
    args[idx++] = "-c";
    args[idx++] = "-pipe";
    args[idx++] = job->src;
    args[idx++] = "-o";
    args[idx++] = job->out_file;

    // append flags
    if (ctx->flags)
    {
        for (const char **f = ctx->flags; *f; ++f)
        {
            args[idx++] = (char *) *f;
        }
    }

    // append includes
    if (ctx->includes)
    {
        for (const char **inc = ctx->includes; *inc; ++inc)
        {
            args[idx++] = (char *) *inc;
        }
    }

    // append defines
    if (ctx->defines)
    {
        for (const char **d = ctx->defines; *d; ++d)
        {
            args[idx++] = (char *) *d;
        }
    }

    if (ctx->pch_out[0] != '\0')
    {
        args[idx++] = "-include";
        args[idx++] = (char *) ctx->pch_file;
    }

    args[idx] = NULL;
    assert(idx < argc_total && "args overflow");

    if (BARR_run_process(args[0], args, false) != 0)
    {
        BARR_errlog("Compilation failed for: %s", job->src);
    }
    else
    {
        size_t done = atomic_fetch_add(&job->progress_ctx->completed, 1) + 1;

        pthread_mutex_lock(&job->progress_ctx->log_mutex);

        printf("\r[%zu/%zu] >>>> %s", done, job->progress_ctx->total, job->src);
        fflush(stdout);

        pthread_mutex_unlock(&job->progress_ctx->log_mutex);
    }

    free(args);
    free(job->src);
    free(job);
}

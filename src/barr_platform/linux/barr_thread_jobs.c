// Created by Build Barrage
#include "barr_thread_jobs.h"
#include "barr_io.h"

void BARR_write_sources_job(void *arg)
{
    BARR_WriteSourceArgs *args = (BARR_WriteSourceArgs *) arg;

    BARR_file_write(args->filepath, "%s\n", "# Source files");
    for (size_t i = 0; i < args->sources->count; i++)
    {
        BARR_file_append(args->filepath, "%s\n", args->sources->entries[i]);
    }
}

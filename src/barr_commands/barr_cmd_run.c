#include "barr_cmd_run.h"
#include "barr_cmd_version.h"
#include "barr_gc.h"
#include "barr_io.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

barr_i32 BARR_command_run(barr_i32 argc, char **argv)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (!BARR_is_initialized())
    {
        return 1;
    }

    char last_bin_path[BARR_PATH_MAX] = {0};
    char cache_file[BARR_PATH_MAX];
    snprintf(cache_file, BARR_PATH_MAX, ".barr/data/last_bin");

    if (!BARR_isfile(cache_file))
    {
        BARR_errlog("No build found. Run 'barr build` first");
        return 1;
    }

    FILE *fp = fopen(cache_file, "r");
    if (!fp)
    {
        BARR_errlog("%s(): failed to open %s file", __func__, cache_file);
        return 1;
    }

    if (!fgets(last_bin_path, sizeof(last_bin_path), fp))
    {
        fclose(fp);
        BARR_errlog("%s(): failed to read last build path from %s", __func__, cache_file);
        return 1;
    }
    fclose(fp);

    size_t len = strlen(last_bin_path);
    if (len && last_bin_path[len - 1] == '\n')
    {
        last_bin_path[len - 1] = '\0';
    }

    if (!BARR_isfile(last_bin_path))
    {
        BARR_errlog("%s(): last build binary path does not exist: %s", __func__, last_bin_path);
        return 1;
    }

    char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 1));
    exec_args[0] = last_bin_path;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        exec_args[i] = argv[i];
    }
    exec_args[argc] = NULL;

    BARR_printf("================================================================================\n");
    BARR_log("Running: %s", exec_args[0]);
    BARR_printf("\n\n");
    barr_i32 ret = BARR_run_process(last_bin_path, exec_args, false);

    //----------------------------------------------------------------------------------------------------
    // CLOCK
    clock_gettime(CLOCK_MONOTONIC, &end);
    barr_i64 sec = (barr_i64) (end.tv_sec - start.tv_sec);
    barr_i64 nsec = (barr_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    BARR_printf("\n\n");
    BARR_log("Run %s exited with (%d) code: \033[34;1m %.6fs", exec_args[0], ret, elapsed);
    BARR_printf("================================================================================\n");
    return ret;
}

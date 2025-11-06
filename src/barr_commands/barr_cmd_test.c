#include "barr_cmd_test.h"
#include "barr_env.h"
#include "barr_glob_config_keys.h"
#include "barr_glob_config_parser.h"
#include "barr_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

barr_i32 BARR_command_test(barr_i32 argc, char **argv)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    /*
    for (barr_i32 i = 1; i < argc; i++)
    {
        continue;
    }
    */

    if (argc < 3 || !BARR_strmatch(argv[1], "--create"))
    {
        BARR_errlog("Usage: barr test --create <num_files> [dir] [--verbose|-v]");
        return 1;
    }

    char *endptr;
    barr_i64 n = strtoll(argv[2], &endptr, 10);
    if (*endptr != '\0' || n < 1 || n > BARR_TEST_CREATE_MAX_N_FILES)
    {
        BARR_errlog("Invalid file count: %s (must be 1 to %lld)", argv[2], BARR_TEST_CREATE_MAX_N_FILES);
        return 1;
    }

    bool verbose = false;
    bool no_confirm = false;
    const char *dirpath = NULL;

    for (barr_i32 i = 3; i < argc; i++)
    {
        if (BARR_strmatch(argv[i], "--verbose") || BARR_strmatch(argv[i], "-v"))
        {
            verbose = true;
        }
        else if (BARR_strmatch(argv[i], "--no-confirm") || BARR_strmatch(argv[i], "-nc"))
        {
            no_confirm = true;
        }
        else
        {
            dirpath = argv[i];
        }
    }

    char *allocated_dirpath = NULL;
    if (!dirpath)
    {
        const char *cwd = BARR_getcwd();
        if (!cwd)
        {
            BARR_errlog("Failed to get current working directory");
            return 1;
        }
        size_t len = strlen(cwd) + strlen("/src") + 1;
        allocated_dirpath = malloc(len);
        if (!allocated_dirpath)
        {
            BARR_errlog("Failed to allocate memory for dirpath");
            return 1;
        }
        snprintf(allocated_dirpath, len, "%s/src", cwd);
        dirpath = allocated_dirpath;
    }

    // prompt
    if (!no_confirm)
    {
        BARR_printf("Create %ld files in \"%s\"? [y/N]: ", n, dirpath);
        barr_i32 c = getchar();
        if (c != 'y' && c != 'Y')
        {
            BARR_warnlog("Aborted");
            if (allocated_dirpath)
            {
                free(allocated_dirpath);
                return 0;
            }
        }

        // consume remaining input
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
    }

    if (!BARR_isdir(dirpath))
    {
        if (barr_mkdir(dirpath))
        {
            BARR_errlog("%s(): barr_mkdir(%s) failed", __func__, dirpath);
            if (allocated_dirpath)
            {
                free(allocated_dirpath);
            }
            return 1;
        }
    }

    char filepath[BARR_PATH_MAX];
    FILE *fp = NULL;
    for (barr_i64 i = 1; i <= n; i++)
    {
        snprintf(filepath, sizeof(filepath), "%s/file%ld.c", dirpath, i);
        fp = fopen(filepath, "w");
        if (!fp)
        {
            BARR_errlog("%s(): failed to create %s", __func__, filepath);
            if (allocated_dirpath)
            {
                free(allocated_dirpath);
            }
            return 1;
        }
        fprintf(
            fp,
            "#include <stdio.h>\nint function%ld(void) {\n    printf(\"Hello, file%ld.c!\\n\");\n    return 0;\n}\n", i,
            i);
        fclose(fp);

        if (verbose)
        {
            printf("\r%ld/%ld: %s", i, n, filepath);
            fflush(stdout);
        }
    }

    // Log performance
    clock_gettime(CLOCK_MONOTONIC, &end);
    barr_i64 sec = (barr_i64) (end.tv_sec - start.tv_sec);
    barr_i64 nsec = (barr_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        sec--;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    printf("\n");
    BARR_log("Created %ld files (%.2f files/sec) in %.6f sec", n, (double) n / elapsed, elapsed);

    if (allocated_dirpath)
    {
        free(allocated_dirpath);
    }
    return 0;
}

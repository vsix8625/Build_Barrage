#include "atl_cmd_test.h"
#include "atl_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

atl_i32 ATL_command_test(atl_i32 argc, char **argv)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (argc < 3 || !ATL_strmatch(argv[1], "--create"))
    {
        ATL_errlog("Usage: atl test --create <num_files> [dir] [--verbose|-v]");
        return 1;
    }

    char *endptr;
    atl_i64 n = strtoll(argv[2], &endptr, 10);
    if (*endptr != '\0' || n < 1 || n > ATL_TEST_CREATE_MAX_N_FILES)
    {
        ATL_errlog("Invalid file count: %s (must be 1 to %lld)", argv[2], ATL_TEST_CREATE_MAX_N_FILES);
        return 1;
    }

    bool verbose = false;
    bool no_confirm = false;
    const char *dirpath = NULL;

    for (atl_i32 i = 3; i < argc; i++)
    {
        if (ATL_strmatch(argv[i], "--verbose") || ATL_strmatch(argv[i], "-v"))
        {
            verbose = true;
        }
        else if (ATL_strmatch(argv[i], "--no-confirm") || ATL_strmatch(argv[i], "-nc"))
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
        const char *cwd = ATL_getcwd();
        if (!cwd)
        {
            ATL_errlog("Failed to get current working directory");
            return 1;
        }
        size_t len = strlen(cwd) + strlen("/src") + 1;
        allocated_dirpath = malloc(len);
        if (!allocated_dirpath)
        {
            ATL_errlog("Failed to allocate memory for dirpath");
            return 1;
        }
        snprintf(allocated_dirpath, len, "%s/src", cwd);
        dirpath = allocated_dirpath;
    }

    // prompt
    if (!no_confirm)
    {
        ATL_printf("Create %ld files in \"%s\"? [y/N]: ", n, dirpath);
        atl_i32 c = getchar();
        if (c != 'y' && c != 'Y')
        {
            ATL_warnlog("Aborted");
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

    if (!ATL_isdir(dirpath))
    {
        if (atl_mkdir(dirpath))
        {
            ATL_errlog("%s(): atl_mkdir(%s) failed", __func__, dirpath);
            if (allocated_dirpath)
            {
                free(allocated_dirpath);
            }
            return 1;
        }
    }

    char filepath[ATL_PATH_MAX];
    FILE *fp = NULL;
    for (atl_i64 i = 1; i <= n; i++)
    {
        snprintf(filepath, sizeof(filepath), "%s/file%ld.c", dirpath, i);
        fp = fopen(filepath, "w");
        if (!fp)
        {
            ATL_errlog("%s(): failed to create %s", __func__, filepath);
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
    atl_i64 sec = (atl_i64) (end.tv_sec - start.tv_sec);
    atl_i64 nsec = (atl_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        sec--;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    printf("\n");
    ATL_log("Created %ld files (%.2f files/sec) in %.6f sec", n, (double) n / elapsed, elapsed);

    if (allocated_dirpath)
    {
        free(allocated_dirpath);
    }
    return 0;
}

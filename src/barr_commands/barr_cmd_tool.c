#include "barr_cmd_tool.h"
#include "barr_gc.h"
#include "barr_io.h"
#include <stdio.h>
#include <string.h>

// TODO: idx count for args
barr_i32 BARR_command_tool(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("Usage: barr tool --gdb|--valgrind|--strace [args...]");
        return 1;
    }

    const char *cache_file = ".barr/data/last_bin";
    char last_bin_path[BARR_BUF_SIZE_256] = {0};

    // read last_bin_path like run
    FILE *fp = fopen(cache_file, "r");
    if (!fp)
    {
        BARR_errlog("Cannot open last_bin cache");
        return 1;
    }
    if (!fgets(last_bin_path, sizeof(last_bin_path), fp))
    {
        fclose(fp);
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
        BARR_errlog("Last build binary does not exist: %s", last_bin_path);
        return 1;
    }

    // dispatch loop
    for (barr_i32 i = 1; i < argc; ++i)
    {
        if (BARR_strmatch(argv[i], "--gdb"))
        {
            if (!BARR_is_installed("gdb"))
            {
                BARR_errlog("GDB not found");
                return 1;
            }

            BARR_log("Launching GDB for last built binary: %s", last_bin_path);

            // build exec args
            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 1));
            exec_args[0] = "gdb";
            exec_args[1] = last_bin_path;

            // pass remaining user args
            barr_i32 j = 2;
            for (barr_i32 k = i + 1; k < argc; ++k, ++j)
            {
                exec_args[j] = argv[k];
            }
            exec_args[j] = NULL;

            BARR_run_process(exec_args[0], exec_args, false);
            return 0;
        }
        else if (BARR_strmatch(argv[i], "--valgrind"))
        {
            if (!BARR_is_installed("valgrind"))
            {
                BARR_errlog("Valgrind not found");
                return 1;
            }

            BARR_log("Launching Valgrind for last built binary: %s", last_bin_path);

            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 2));
            exec_args[0] = "valgrind";
            exec_args[1] = "--tool=memcheck";
            exec_args[2] = last_bin_path;

            barr_i32 j = 3;
            for (barr_i32 k = i + 1; k < argc; ++k, ++j)
            {
                exec_args[j] = argv[k];
            }
            exec_args[j] = NULL;

            BARR_run_process(exec_args[0], exec_args, false);
            return 0;
        }
        else if (BARR_strmatch(argv[i], "--strace"))
        {
            if (!BARR_is_installed("strace"))
            {
                BARR_errlog("strace not found");
                return 1;
            }

            BARR_log("Launching strace for last built binary: %s", last_bin_path);

            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 2));
            exec_args[0] = "strace";
            exec_args[1] = "-c";
            exec_args[2] = last_bin_path;

            barr_i32 j = 3;
            for (barr_i32 k = i + 1; k < argc; ++k, ++j)
            {
                exec_args[j] = argv[k];
            }
            exec_args[j] = NULL;

            BARR_run_process(exec_args[0], exec_args, false);
            return 0;
        }
    }

    BARR_errlog("Unknown tool flag. Usage: barr tool --gdb|--valgrind|--strace");
    return 1;
}

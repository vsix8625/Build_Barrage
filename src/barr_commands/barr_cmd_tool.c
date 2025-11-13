#include "barr_cmd_tool.h"
#include "barr_gc.h"
#include "barr_io.h"
#include <stdio.h>
#include <string.h>

barr_i32 BARR_command_tool(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("Usage: barr tool --gdb|--valgrind|--strace [args...]");
        return 1;
    }

    char *target_name = BARR_get_build_info_key(BARR_DATA_BUILD_INFO_PATH, "name");
    char *build_dir = BARR_get_build_info_key(BARR_DATA_BUILD_INFO_PATH, "build_dir");

    char exe_path[BARR_PATH_MAX];
    snprintf(exe_path, sizeof(exe_path), "%s/bin/%s", build_dir, target_name);

    char **executable = BARR_gc_alloc(sizeof(char *) * (argc + 1));
    executable[0] = exe_path;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        executable[i] = argv[i];
    }
    executable[argc] = NULL;

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

            BARR_log("Launching GDB for last built binary: %s", executable[0]);

            // build exec args
            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 1));
            exec_args[0] = "gdb";
            exec_args[1] = executable[0];

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

            BARR_log("Launching Valgrind for last built binary: %s", executable[0]);

            char *valgrind = BARR_which("valgrind");

            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 2));
            exec_args[0] = valgrind;
            exec_args[1] = "--tool=memcheck";
            exec_args[2] = executable[0];

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

            char output_file_path[BARR_PATH_MAX];
            snprintf(output_file_path, sizeof(output_file_path), "%s_strace_syscall.txt", target_name);

            BARR_log("Launching strace for last built binary: %s", executable[0]);

            char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 2));
            barr_i32 idx = 0;

            exec_args[idx++] = "strace";
            exec_args[idx++] = "-c";
            exec_args[idx++] = "-o";
            exec_args[idx++] = output_file_path;
            exec_args[idx++] = executable[0];

            for (barr_i32 k = i + 1; k < argc; ++k, idx++)
            {
                exec_args[idx] = argv[k];
            }
            exec_args[idx] = NULL;

            BARR_run_process(exec_args[0], exec_args, false);
            return 0;
        }
    }

    BARR_errlog("Unknown tool flag. Usage: barr tool --gdb|--valgrind|--strace");
    return 1;
}

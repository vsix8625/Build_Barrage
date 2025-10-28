#include "barr_cmd_tool.h"
#include "barr_gc.h"
#include "barr_io.h"
#include <stdio.h>
#include <string.h>

barr_i32 BARR_command_tool(barr_i32 argc, char **argv)
{
    if (!BARR_is_installed("gdb"))
    {
        BARR_errlog("%s(): GDB not found in $PATH, to use this command please install it", __func__);
        return 1;
    }

    const char *cache_file = ".barr/data/last_bin";
    char last_bin_path[BARR_PATH_MAX] = {0};

    FILE *fp = fopen(cache_file, "r");
    if (fp == NULL)
    {
        BARR_errlog("%s(): cannot open cache file: %s", __func__, cache_file);
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
        BARR_errlog("%s(): binary path does not exist: %s", __func__, last_bin_path);
        return 1;
    }

    BARR_log("Launching GDB for last built binary: %s", last_bin_path);

    char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 2));
    exec_args[0] = "gdb";
    exec_args[1] = last_bin_path;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        exec_args[i + 1] = argv[i];
    }
    exec_args[argc + 1] = NULL;

    BARR_run_process(exec_args[0], exec_args, false);

    return 0;
}

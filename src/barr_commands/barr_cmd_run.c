#include "barr_cmd_run.h"
#include "barr_cmd_version.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos_ast.h"
#include "olmos_variables.h"

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

    // -------------------------------------------------------------------------------

    OLM_init();
    OLM_AST_Node *root = OLM_parse_file("Barrfile");
    if (root == NULL)
    {
        BARR_errlog("%s(): failed to parse Barrfile", __func__);
        return 1;
    }

    OLM_parse_vars(root);

    const char *vers = OLM_get_var(OLM_VAR_VERSION);
    if (vers == NULL)
    {
        vers = "0.0.1";
        BARR_log("Version not set in 'Barrfile' default %s will be used", vers);
    }
    OLM_close();

    // -------------------------------------------------------------------------------

    BARR_printf("================================================================================\n");
    BARR_log("Running: %s-v%s", exec_args[0], vers);
    BARR_printf("\n\n");
    barr_i32 ret = BARR_run_process(last_bin_path, exec_args, false);

    //----------------------------------------------------------------------------------------------------
    // CLOCK
    clock_gettime(CLOCK_MONOTONIC, &end);
    BARR_printf("\n\n");
    char ret_color_buf[BARR_BUF_SIZE_32];
    if (ret)
    {
        snprintf(ret_color_buf, sizeof(ret_color_buf), "\033[31;1m%d\033[32;1m", ret);
    }
    else
    {
        snprintf(ret_color_buf, sizeof(ret_color_buf), "\033[34;1m%d\033[32;1m", ret);
    }
    BARR_log("Run %s exited with (%s) code: \033[34;1m %s", exec_args[0], ret_color_buf,
             BARR_fmt_time_elapsed(&start, &end));
    BARR_printf("================================================================================\n");
    return ret;
}

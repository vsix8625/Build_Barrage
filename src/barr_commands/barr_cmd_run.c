#include "barr_cmd_run.h"
#include "barr_cmd_version.h"
#include "barr_gc.h"
#include "barr_io.h"
#include "olmos_ast.h"
#include "olmos_variables.h"

#include <stdio.h>
#include <time.h>

barr_i32 BARR_command_run(barr_i32 argc, char **argv)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (!BARR_init())
    {
        return 1;
    }

    char *target_name = BARR_get_build_info_key(BARR_DATA_BUILD_INFO_PATH, "name");
    char *build_dir = BARR_get_build_info_key(BARR_DATA_BUILD_INFO_PATH, "build_dir");

    char exe_path[BARR_PATH_MAX];
    snprintf(exe_path, sizeof(exe_path), "%s/bin/%s", build_dir, target_name);

    char **exec_args = BARR_gc_alloc(sizeof(char *) * (argc + 1));
    exec_args[0] = exe_path;
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
    barr_i32 ret = BARR_run_process(exec_args[0], exec_args, false);

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

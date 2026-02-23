#include "barr_cmd_clean.h"
#include "barr_env.h"
#include "barr_io.h"
#include "olmos.h"
#include "olmos_variables.h"
#include <stdio.h>

barr_i32 BARR_command_clean(barr_i32 argc, char **argv)
{
    if (!BARR_init())
    {
        return 1;
    }

    bool no_confirm = false;
    for (barr_i32 i = 1; i < argc; ++i)
    {
        char *cmd = argv[i];
        if (BARR_strmatch(cmd, "--no-confirm") || BARR_strmatch(cmd, "-nc"))
        {
            no_confirm = true;
        }
        else
        {
            BARR_warnlog("Invalid option for clean command: %s", cmd);
        }
    }

    if (!no_confirm && !g_barr_silent_logs)
    {
        BARR_printf("Are you sure you want to clean? [y/N]: ");
        barr_i32 c = getchar();
        if (c != 'y' && c != 'Y')
        {
            BARR_warnlog("Clean aborted!");
            return 0;
        }
        while ((c = getchar()) != '\n' && c != EOF)
        {
        }
    }

    // --------------------------------------------------------------------
    // Clean dirs specified in Barrfile clean_dirs variable

    OLM_init();
    OLM_Node *root = OLM_parse_file("Barrfile");
    if (root == NULL)
    {
        BARR_errlog("%s(): failed to parse Barrfile", __func__);
        return 1;
    }

    OLM_parse_vars(root);

    const char *out_dir = OLM_get_var(OLM_VAR_OUT_DIR);
    if (out_dir)
    {
        if (BARR_isdir(out_dir))
        {
            BARR_rmrf(out_dir);
            BARR_log("Removed directory: %s", out_dir);
        }
    }

    // clean_dirs
    const char *clean_dirs_var = OLM_get_var(OLM_VAR_CLEAN_TARGETS);
    if (clean_dirs_var)
    {
        char **dirs = BARR_tokenize_string(clean_dirs_var);
        if (dirs)
        {
            for (size_t i = 0; dirs[i]; i++)
            {
                const char *idx = dirs[i];
                if (BARR_isdir(idx))
                {
                    BARR_rmrf(idx);
                    BARR_log("Removed directory: %s", idx);
                }
                else if (BARR_isfile(idx))
                {
                    BARR_rmrf(idx);
                    BARR_log("Removed file: %s", idx);
                }
            }
        }
    }
    OLM_close();

    if (BARR_isdir("build"))
    {
        BARR_rmrf("build");
        BARR_log("Removed directory: build");
    }

    if (BARR_isfile(BARR_CACHE_FILE))
    {
        BARR_rmrf(BARR_CACHE_FILE);
        BARR_log("Removed file: %s", BARR_CACHE_FILE);
    }

    BARR_log("Clean completed!");
    return 0;
}

#include "barr_cmd_config_linux.h"
#include "barr_env.h"
#include "barr_io.h"

#include <string.h>

//----------------------------------------------------------------------------------------------------

static barr_i32 barr_config_local_edit(barr_i32 argc, char **argv)
{
    BARR_VOID(argc);
    BARR_VOID(argv);
    if (!BARR_is_initialized())
    {
        return 1;
    }

    char *args[] = {(char *) BARR_get_config("editor"), "Barrfile", NULL};
    BARR_run_process(args[0], args, false);
    return 0;
}

//----------------------------------------------------------------------------------------------------

barr_i32 BARR_cmd_config(barr_i32 argc, char **argv)
{
    if (argc < 2)
    {
        BARR_errlog("%s: config command requires option", __func__);
        return 1;
    }

    for (barr_i32 i = 1; i < argc; ++i)
    {
        if (BARR_strmatch(argv[i], "--barrfile") || BARR_strmatch(argv[i], "-bf"))
        {
            BARR_log("%s: %s/Barrfile", BARR_get_config("editor"), BARR_getcwd());
            return barr_config_local_edit(argc, argv);
        }
    }

    BARR_errlog("%s: Unknown option for config command", __func__);
    return 1;
}

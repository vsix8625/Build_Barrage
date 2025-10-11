#include "atl_cmd_build.h"
#include "atl_debug.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"

#include <inttypes.h>
#include <lauxlib.h>
#include <lualib.h>
#include <time.h>

atl_i32 ATL_command_build(atl_i32 argc, char **argv)
{
    ATL_VOID(argc);
    ATL_VOID(argv);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    if (!ATL_is_initialized())
    {
        return 1;
    }

    if (!ATL_isdir("build"))
    {
        if (atl_mkdir("build"))
        {
            ATL_errlog("Failed to create build directory");
            ATL_dumb_backtrace();
            return 1;
        }
    }

    //  parse global config
    const char *rc_file = ATL_get_config("file");
    ATL_ConfigTable *rc_table = ATL_config_parse_file(rc_file);

    //----------------------------------------------------------------------------------------------------
    // CLOCK
    clock_gettime(CLOCK_MONOTONIC, &end);
    atl_i64 sec = (atl_i64) (end.tv_sec - start.tv_sec);
    atl_i64 nsec = (atl_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    ATL_log("Build completed in: \033[34;1m %.6fs", elapsed);

    // cleanup
    rc_table->destroy(rc_table);
    return 0;
}

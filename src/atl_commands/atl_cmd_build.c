#include "atl_cmd_build.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"
#include "atl_lua_rt.h"

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
    // TODO: --path arg for remote build
    const char *rc_file = ATL_get_config("file");
    ATL_ConfigTable *rc_table = ATL_config_parse_file(rc_file);

    // test
    ATL_ConfigEntry *atl_root_dir_entry = ATL_config_table_get(rc_table, ATL_GLOB_CONFIG_KEY_ROOT_DIR);
    char *test = (atl_root_dir_entry && atl_root_dir_entry->value.str_val) ? atl_root_dir_entry->value.str_val : "N/A";
    ATL_log("value=%s", test);

    //----------------------------------------------------------------------------------------------------
    // LUA

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    // initialize lua modules
    ATL_l_init_os(L);

    if (ATL_isfile(ATL_BUILD_FILE_L))
    {
        // TODO: a lua hook with timeout
        if (luaL_dofile(L, ATL_BUILD_FILE_L))
        {
            ATL_errlog("Lua error: %s", lua_tostring(L, -1));
        }
    }
    else
    {
        ATL_warnlog("Could not locate %s file", ATL_BUILD_FILE_L);
        // TODO: create a def one
    }

    //----------------------------------------------------------------------------------------------------

    // --------CLOCK---------------------------------------------
    clock_gettime(CLOCK_MONOTONIC, &end);
    atl_i64 sec = (atl_i64) (end.tv_sec - start.tv_sec);
    atl_i64 nsec = (atl_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    ATL_log("Build completed in:\033[34;1m %" PRId64 ".%04" PRId64 "s", sec, nsec);

    lua_close(L);
    rc_table->destroy(rc_table);
    return 0;
}

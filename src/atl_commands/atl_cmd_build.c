#include "atl_cmd_build.h"
#include "atl_debug.h"
#include "atl_depgen.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"
#include "atl_lua_rt.h"
#include "atl_src_list.h"
#include "atl_src_scan.h"

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

    // TODO: --path arg for remote build
    const char *rc_file = ATL_get_config("file");
    ATL_ConfigTable *rc_table = ATL_config_parse_file(rc_file);

    // test
    ATL_ConfigEntry *atl_root_dir_entry = ATL_config_table_get(rc_table, ATL_GLOB_CONFIG_KEY_ROOT_DIR);
    char *test = (atl_root_dir_entry && atl_root_dir_entry->value.str_val) ? atl_root_dir_entry->value.str_val : "N/A";
    ATL_log("value=%s", test);

    //----------------------------------------------------------------------------------------------------
    // LUA

    ATL_LuaCtx lua_ctx;
    ATL_l_ctx_init(&lua_ctx);

    static ATL_LuaField atl_l_fields[] = {{"os", "info", ATL_l_os_info},
                                          {"env", "get", ATL_l_env_get},
                                          {"env", "set", ATL_l_env_set},
                                          {"env", "all", ATL_l_env_all},
                                          {NULL, NULL, NULL}};

    // register all lua fields
    for (ATL_LuaField *f = atl_l_fields; f->module; f++)
    {
        ATL_l_register_field(&lua_ctx, f->module, f->name, f->fn);
    }

    // finalize
    ATL_l_finalize_fields(&lua_ctx);

    if (ATL_isfile(ATL_BUILD_FILE_L))
    {
        // TODO: a lua hook with timeout
        if (luaL_dofile(lua_ctx.L, ATL_BUILD_FILE_L))
        {
            ATL_errlog("Lua error: %s", lua_tostring(lua_ctx.L, -1));
            ATL_dumb_backtrace();
            lua_pop(lua_ctx.L, 1);
            // continue;
        }
    }
    else
    {
        ATL_warnlog("Could not locate %s file", ATL_BUILD_FILE_L);
        // TODO: create a def one
    }

    //----------------------------------------------------------------------------------------------------
    // DEPGEN

    ATL_SourceList src_list = {0};
    size_t src_list_max_files = 10000;
    ATL_Arena src_list_arena = {0};
    size_t avg_path_len = ATL_BUF_SIZE_256;
    size_t src_list_arena_size = src_list_max_files * (sizeof(char *) + avg_path_len);

    // init
    ATL_arena_init(&src_list_arena, src_list_arena_size, "src_list_arena");
    ATL_source_list_init(&src_list, &src_list_arena, src_list_max_files);

    ATL_source_list_scan_dir(&src_list, "src", ATL_getcwd());
    ATL_sources_get_deps(&src_list);

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
    ATL_l_destroy_ctx(&lua_ctx);
    ATL_destroy_arena(&src_list_arena);
    rc_table->destroy(rc_table);
    return 0;
}

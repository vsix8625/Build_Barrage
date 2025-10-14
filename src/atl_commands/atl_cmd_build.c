#include "atl_cmd_build.h"
#include "atl_debug.h"
#include "atl_env.h"
#include "atl_glob_config_keys.h"
#include "atl_glob_config_parser.h"
#include "atl_io.h"
#include "atl_src_list.h"
#include "atl_src_scan.h"
#include "crx.h"
#include "crx_ast.h"

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

    bool has_cache = false;
    if (ATL_isfile(ATL_CACHE_FILE))
    {
        has_cache = true;
    }

    //  parse global config
    const char *rc_file = ATL_get_config("file");
    ATL_ConfigTable *rc_table = ATL_config_parse_file(rc_file);

    //----------------------------------------------------------------------------------------------------
    // Crux

    if (!CRX_init())
    {
        ATL_errlog("%s(): failed to initialize crux", __func__);
        rc_table->destroy(rc_table);
        return 1;
    }

    CRX_AST_Node *root = crx_test_ast();
    CRX_eval_node(root);

    //----------------------------------------------------------------------------------------------------
    // Begin build

    if (has_cache)
    {
        ATL_warnlog("Cache exists");
        // compare hashmap with current project scan?
    }

    const char *scan_dir = ATL_getcwd();
    ATL_SourceList list;
    if (!ATL_source_list_init(&list, ATL_SOURCE_LIST_INITIAL_FILES))
    {
        ATL_errlog("%s(): failed to initialize source list.", __func__);
        CRX_close();
        rc_table->destroy(rc_table);
        return 1;
    }
    ATL_source_list_scan_dir(&list, scan_dir);

    ATL_HashMap *map = ATL_hashmap_create(ATL_BUF_SIZE_8192);
    ATL_source_list_hash(&list, map, "-Wall");
    ATL_hashmap_debug(map);

    const char *cache_file = ATL_CACHE_FILE;
    if (!ATL_hashmap_write_cache(map, cache_file))
    {
        ATL_errlog("%s(): failed to write cache", __func__);
    }

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
    CRX_close();
    ATL_destroy_source_list(&list);
    ATL_destroy_hashmap(map);
    rc_table->destroy(rc_table);
    return 0;
}

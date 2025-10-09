#include "atl_debug.h"
#include "atl_io.h"
#include "atl_lua_rt.h"
#include "atl_src_list.h"
#include "atl_src_scan.h"

#include <lauxlib.h>
#include <stdlib.h>
#include <time.h>

atl_i32 ATL_l_list_get_src(lua_State *L)
{
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    const char *dirpath = NULL;
    const char *project_root = NULL;

    atl_i32 argc = lua_gettop(L);

    if (argc == 0)
    {
        dirpath = ATL_getcwd();
    }
    else
    {
        // dirpath passed as arg
        dirpath = luaL_checkstring(L, 1);
        if (argc > 2 && !lua_isnil(L, 2))
        {
            project_root = luaL_checkstring(L, 2);
        }
    }

    ATL_SourceList list = {0};
    ATL_source_list_init(&list, ATL_SOURCE_LIST_INITIAL_FILES);
    ATL_source_list_scan_dir(&list, dirpath, project_root);

    lua_newtable(L);
    for (size_t i = 0; i < list.count; i++)
    {
        if (!list.entries[i])
        {
            // ATL_warnlog("entry %zu is NULL", i);
            continue;
        }

        lua_pushstring(L, list.entries[i]);
        lua_rawseti(L, -2, i + 1);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    atl_i64 sec = (atl_i64) (end.tv_sec - start.tv_sec);
    atl_i64 nsec = (atl_i64) (end.tv_nsec - start.tv_nsec);
    if (nsec < 0)
    {
        --sec;
        nsec += 1000000000LL;
    }
    double elapsed = (double) sec + (double) nsec / 1e9;
    ATL_log("Scanned: %zu files (%.2f files per sec)", list.count, (double) list.count / elapsed);
    ATL_log("Scan completed in: \033[34;1m %.6fs", elapsed);

    // cleanup
    ATL_destroy_source_list(&list);
    return 1;
}

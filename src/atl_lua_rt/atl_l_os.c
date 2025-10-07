#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "atl_env.h"
#include "atl_io.h"
#include "atl_lua_rt.h"

atl_i32 ATL_l_os_info(lua_State *L)
{
    lua_newtable(L);

    lua_pushstring(L, ATL_OS_NAME);
    lua_setfield(L, -2, "name");

    lua_pushstring(L, ATL_get_config("version"));
    lua_setfield(L, -2, "version");

    lua_pushstring(L, ATL_get_config("machine"));
    lua_setfield(L, -2, "machine");

    lua_pushinteger(L, ATL_OS_GET_CORES());
    lua_setfield(L, -2, "cores");

    lua_pushinteger(L, ATL_OS_GET_CACHE_LINE_SIZE());
    lua_setfield(L, -2, "cache_line_size");

    atl_i32 little_endian = (*(atl_u16 *) "\1\0" == 1);
    lua_pushboolean(L, little_endian);
    lua_setfield(L, -2, "little_endian");
    lua_pushboolean(L, !little_endian);
    lua_setfield(L, -2, "big_endian");

    char hostname[ATL_BUF_SIZE_256];
    if (gethostname(hostname, sizeof(hostname)) == 0)
    {
        lua_pushstring(L, hostname);
        lua_setfield(L, -2, "hostname");
    }

    return 1;
}

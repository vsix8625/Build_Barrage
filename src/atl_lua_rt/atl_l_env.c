#include "atl_lua_rt.h"
#include "atl_utils.h"

#include <errno.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

atl_i32 ATL_l_env_get(lua_State *L)
{
    const char *var = luaL_checkstring(L, 1);
    const char *val = getenv(var);

    if (val)
    {
        lua_pushstring(L, val);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}

atl_i32 ATL_l_env_set(lua_State *L)
{
    const char *var = luaL_checkstring(L, 1);
    const char *val = luaL_checkstring(L, 2);

    if (setenv(var, val, 1) != 0)
    {
        lua_pushboolean(L, 0);  // failed
        lua_pushstring(L, strerror(errno));
        return 2;
    }

    lua_pushboolean(L, 1);  // success
    return 1;
}

extern char **environ;

atl_i32 ATL_l_env_all(lua_State *L)
{
    lua_newtable(L);
    for (char **e = environ; *e; e++)
    {
        char *eq = strrchr(*e, '=');
        if (!eq)
        {
            continue;
        }

        *eq = 0;
        const char *key = *e;
        const char *value = eq + 1;

        lua_pushstring(L, value);
        lua_setfield(L, -2, key);

        *eq = '=';
    }
    return 1;
}

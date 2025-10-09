#include "atl_io.h"
#include "atl_lua_rt.h"
#include "atl_project.h"

#include <lauxlib.h>

atl_i32 ATL_l_project_create(lua_State *L)
{
    // stack: project { name=..., root_dir=..., build_type=..., compile_flags=... }
    if (!lua_istable(L, 1))
    {
        lua_pushnil(L);
        lua_pushstring(L, "Expected table as argument");
        return 2;
    }

    lua_getfield(L, 1, "name");
    const char *name = luaL_optstring(L, -1, ATL_PROJECT_DEFAULT_NAME);
    lua_pop(L, 1);

    lua_getfield(L, 1, "root_dir");
    const char *root_dir = luaL_optstring(L, -1, ATL_getcwd());
    lua_pop(L, 1);

    lua_getfield(L, 1, "build_type");
    const char *build_type = luaL_optstring(L, -1, ATL_PROJECT_DEFAULT_BUILD_TYPE);
    lua_pop(L, 1);

    lua_getfield(L, 1, "compile_flags");
    const char *compile_flags = luaL_optstring(L, -1, ATL_PROJECT_DEFAULT_CFLAGS);
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, "ATL_PROJECT_LIST");
    ATL_ProjectList *plist = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (!plist)
    {
        lua_pushnil(L);
        lua_pushstring(L, "Project list not initialized");
        return 2;
    }

    ATL_Project *p = ATL_project_list_add(plist, name, root_dir, build_type, compile_flags);
    if (!p)
    {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to create project");
        return 2;
    }

    lua_getfield(L, 1, "source_files");
    p->lua_source_table_ref = LUA_NOREF;
    if (lua_istable(L, -1))
    {
        p->lua_source_table_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    else
    {
        lua_pop(L, 1);
    }

    lua_pushinteger(L, p->id);
    return 1;
}

void ATL_l_project_register_list(lua_State *L, ATL_ProjectList *plist)
{
    lua_pushlightuserdata(L, plist);
    lua_setfield(L, LUA_REGISTRYINDEX, "ATL_PROJECT_LIST");
}

atl_i32 ATL_l_project_get(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, "ATL_PROJECT_LIST");
    ATL_ProjectList *plist = lua_touserdata(L, -1);
    lua_pop(L, 1);
    if (!plist)
    {
        lua_pushnil(L);
        lua_pushstring(L, "Project list not initialized");
        return 2;
    }

    ATL_Project *p = ATL_project_list_get_by_name(plist, name);
    if (!p)
    {
        lua_pushnil(L);
        lua_pushstring(L, "Project not found");
        return 2;
    }

    lua_newtable(L);
    lua_pushstring(L, p->name);
    lua_setfield(L, -2, "name");

    lua_pushstring(L, p->root_dir);
    lua_setfield(L, -2, "root_dir");

    lua_pushstring(L, p->build_type);
    lua_setfield(L, -2, "build_type");

    lua_pushstring(L, p->compile_flags);
    lua_setfield(L, -2, "compile_flags");

    lua_pushinteger(L, p->id);
    lua_setfield(L, -2, "id");

    if (p->lua_source_table_ref != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, p->lua_source_table_ref);
        lua_setfield(L, -2, "source_files");
    }

    return 1;
}

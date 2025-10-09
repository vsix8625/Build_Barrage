#ifndef ATL_LUA_RT_H_
#define ATL_LUA_RT_H_

#include <lua.h>

#include "atl_arena.h"
#include "atl_project.h"

#define ATL_LUA_GLOBAL "atl"

//----------------------------------------------------------------------------------------------------

typedef struct ATL_LuaField
{
    const char *module;
    const char *name;
    lua_CFunction fn;
} ATL_LuaField;

typedef struct ATL_LuaCtx
{
    lua_State *L;
    ATL_Arena arena;
    ATL_LuaField *fields;
    size_t field_count;
} ATL_LuaCtx;

// lua init
void ATL_l_ctx_init(ATL_LuaCtx *ctx);
void ATL_l_destroy_ctx(ATL_LuaCtx *ctx);
void ATL_l_register_field(ATL_LuaCtx *ctx, const char *module, const char *name, lua_CFunction fn);
void ATL_l_finalize_fields(ATL_LuaCtx *ctx);

//----------------------------------------------------------------------------------------------------

// atl.os.info
atl_i32 ATL_l_os_info(lua_State *L);

// atl.env get set all
atl_i32 ATL_l_env_get(lua_State *L);
atl_i32 ATL_l_env_set(lua_State *L);
atl_i32 ATL_l_env_all(lua_State *L);

// atl.list.source_files
atl_i32 ATL_l_list_get_src(lua_State *L);

atl_i32 ATL_l_project_create(lua_State *L);
void ATL_l_project_register_list(lua_State *L, ATL_ProjectList *plist);
atl_i32 ATL_l_project_get(lua_State *L);

#endif  // ATL_LUA_RT_H_

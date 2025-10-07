#include "atl_io.h"
#include "atl_lua_rt.h"
#include <lauxlib.h>
#include <lualib.h>

#define ATL_LUA_MAX_FIELDS 256

void ATL_l_ctx_init(ATL_LuaCtx *ctx)
{
    ATL_arena_init(&ctx->arena, sizeof(ATL_LuaField) * ATL_LUA_MAX_FIELDS, "lua_fields_arena");
    ctx->fields = ATL_arena_alloc(&ctx->arena, sizeof(ATL_LuaField) * ATL_LUA_MAX_FIELDS);
    ctx->field_count = 0;
    ctx->L = luaL_newstate();
    luaL_openlibs(ctx->L);
}

void ATL_l_destroy_ctx(ATL_LuaCtx *ctx)
{
    ATL_destroy_arena(&ctx->arena);
    if (ctx->L)
    {
        lua_close(ctx->L);
    }
}

//----------------------------------------------------------------------------------------------------

void ATL_l_register_field(ATL_LuaCtx *ctx, const char *module, const char *name, lua_CFunction fn)
{
    if (ctx->field_count >= ATL_LUA_MAX_FIELDS)
    {
        ATL_errlog("Allowed number of lua fields registration reached: %d", ATL_LUA_MAX_FIELDS);
        return;
    }

    ATL_LuaField *field = &ctx->fields[ctx->field_count++];
    field->module = module;
    field->name = name;
    field->fn = fn;
}

void ATL_l_finalize_fields(ATL_LuaCtx *ctx)
{
    lua_State *L = ctx->L;

    // load or create global
    lua_getglobal(L, ATL_LUA_GLOBAL);
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, ATL_LUA_GLOBAL);
        lua_getglobal(L, ATL_LUA_GLOBAL);  // reload
    }

    for (size_t i = 0; i < ctx->field_count; i++)
    {
        ATL_LuaField *f = &ctx->fields[i];

        lua_getfield(L, -1, f->module);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
        }

        lua_pushcfunction(L, f->fn);
        lua_setfield(L, -2, f->name);
        lua_setfield(L, -2, f->module);
    }

    lua_pop(L, 1);  // pop atl table
}

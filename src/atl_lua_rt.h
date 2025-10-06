#ifndef ATL_LUA_RT_H_
#define ATL_LUA_RT_H_

#include <lua.h>

#define ATL_LUA_GLOBAL "atl"

// os.info table
void ATL_l_init_os(lua_State *L);

#endif  // ATL_LUA_RT_H_

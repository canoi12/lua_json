#ifndef LUA_JSON_H_
#define LUA_JSON_H_

#define LJSON_API extern

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

LJSON_API int luaopen_json(lua_State* L);

#endif /* LUA_JSON_H_ */

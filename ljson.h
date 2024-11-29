#ifndef LUA_JSON_H_
#define LUA_JSON_H_

#if defined(BUILD_AS_SHARED)
#if defined(_WIN32) || defined(_WIN64)
#define LJSON_API __declspec(dllexport)
#else
#define LJSON_API extern
#endif
#else
#define LJSON_API 
#endif

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

LJSON_API int luaopen_json(lua_State* L);

#endif /* LUA_JSON_H_ */

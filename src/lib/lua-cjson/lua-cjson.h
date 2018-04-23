/*
 * 
 * Copyright (C) 2017-2018 CounterFlow AI, Inc.
 * ===============================================================================
 * 
 * This is a forked, modified version of lua-hiredis.  See the following for 
 * additional information and orginal license:
 * 
 * https://github.com/mpx/lua-cjson
 * 
 * ===============================================================================
 */
/* 
 *
 * author Randy Caldejon <rc@counterflowai.com>
 *
 */

#ifndef _LUA_CJSON_
#define _LUA_CJSON_

LUALIB_API int luaopen_cjson(lua_State * L);
LUALIB_API int luaopen_cjson_safe(lua_State * L);

#endif


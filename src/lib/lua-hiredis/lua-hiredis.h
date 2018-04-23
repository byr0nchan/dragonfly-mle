/*
 * 
 * Copyright (C) 2017-2018 CounterFlow AI, Inc.
 * ===============================================================================
 * 
 * This is a forked, modified version of lua-hiredis.  See the following for 
 * additional information and orginal license:
 * 
 * https://github.com/agladysh/lua-hiredis
 * 
 * ===============================================================================
 */
/* 
 *
 * author Randy Caldejon <rc@counterflowai.com>
 *
 */

#ifndef _LUA_HIREDIS_
#define _LUA_HIREDIS_

LUALIB_API int luaopen_hiredis(lua_State * L, const char *host, int port);

#endif



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

#ifndef _LUA_MSGPACK_
#define _LUA_MSGPACK_

int luaopen_cmsgpack(lua_State *L);
int mp_pack(lua_State *L) ;
int mp_unpack(lua_State *L);
int mp_unpack_one(lua_State *L) ;

#endif
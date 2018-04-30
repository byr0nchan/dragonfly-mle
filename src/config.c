
/* Copyright (C) 2017-2018 CounterFlow AI, Inc.
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/*
 *
 * author Randy Caldejon <rc@counterflowai.com>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#include <lauxlib.h>
#include <luajit-2.0/luajit.h>

#include "config.h"
#include "dragonfly-cmds.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_inputs_config(lua_State *L, INPUT_CONFIG input_list[], int max)
{
    int number_of_inputs = 0;
    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        {.key = "tag", .type = LUA_TSTRING},
        {.key = "uri", .type = LUA_TSTRING},
        {.key = "script", .type = LUA_TSTRING}};

    lua_getglobal(L, "inputs");
    if (lua_type(L, -1) != LUA_TTABLE)
    {
        return number_of_inputs;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    for (int i = 0; i < max; i++)
    {
        lua_rawgeti(L, -1, i + 1);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            break;
        }
        luaL_checktype(L, -1, LUA_TTABLE);

        for (int field_index = 0; field_index < 3; field_index++)
        {
            lua_getfield(L, -1, fields[field_index].key);
            luaL_checktype(L, -1, fields[field_index].type);
            switch (field_index)
            {
            case 0:
            {
                input_list[i].tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG__
                fprintf(stderr, "  [INPUT] tag: %s, ", input_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                input_list[i].uri = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG__
                fprintf(stderr, "uri: %s, ", input_list[i].uri);
#endif
            }
            break;
            case 2:
            {
                struct stat sb;
                const char *path = lua_tostring(L, -1);
                char script_path[PATH_MAX];
                if (*path != '/')
                {
                    snprintf(script_path, PATH_MAX, "%s/%s", ANALYZERS_DIR, path);
                }
                else
                {
                    strncpy(script_path, path, PATH_MAX);
                }
                if ((lstat(script_path, &sb) >= 0) && S_ISREG(sb.st_mode))
                {
                    number_of_inputs++;
                    input_list[i].script = strndup(script_path, PATH_MAX);
#ifdef __DEBUG__
                    fprintf(stderr, "script: %s\n", input_list[i].script);
#endif
                }
                else
                {
                    input_list[i].script = NULL;
                    fprintf(stderr, "script: %s (invalid)\n", input_list[i].script);
                    abort();
                }
            }
            break;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return number_of_inputs;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_analyzers_config(lua_State *L, ANALYZER_CONFIG analyzer_list[], int max)
{
    int number_of_analyzers = 0;
    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        {.key = "tag", .type = LUA_TSTRING},
        {.key = "script", .type = LUA_TSTRING}};

    lua_getglobal(L, "analyzers");
    if (lua_type(L, -1) != LUA_TTABLE)
    {
        return number_of_analyzers;
    }
    luaL_checktype(L, -1, LUA_TTABLE);

    for (int i = 1; i < max; i++)
    {
        lua_rawgeti(L, -1, i);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            break;
        }
        luaL_checktype(L, -1, LUA_TTABLE);

        for (int field_index = 0; field_index < 2; field_index++)
        {
            lua_getfield(L, -1, fields[field_index].key);
            luaL_checktype(L, -1, fields[field_index].type);
            switch (field_index)
            {
            case 0:
            {
                analyzer_list[i - 1].tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG__
                fprintf(stderr, "  [ANALYZER %d : %d] tag: %s, ", i, max, analyzer_list[i - 1].tag);
#endif
            }
            break;
            case 1:
            {
                struct stat sb;
                const char *script_path = lua_tostring(L, -1);
                char lua_analyzer[PATH_MAX];
                if (*script_path != '/')
                {
                    snprintf(lua_analyzer, PATH_MAX, "%s/%s", ANALYZERS_DIR, script_path);
                }
                else
                {
                    strncpy(lua_analyzer, script_path, PATH_MAX);
                }
                if ((lstat(lua_analyzer, &sb) >= 0) && S_ISREG(sb.st_mode))
                {
                    analyzer_list[i - 1].script = strndup(lua_analyzer, PATH_MAX);
#ifdef __DEBUG__
                    fprintf(stderr, "script: %s\n", analyzer_list[i - 1].script);
#endif
                }
                else
                {
                    analyzer_list[i - 1].script = NULL;
                    fprintf(stderr, "script: %s (invalid)\n", analyzer_list[i - 1].script);
                    abort();
                }
            }
            break;
            }
            number_of_analyzers++;
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return number_of_analyzers;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_outputs_config(lua_State *L, OUTPUT_CONFIG output_list[], int max)
{
    int number_of_outputs = 0;
    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        {.key = "tag", .type = LUA_TSTRING},
        {.key = "uri", .type = LUA_TSTRING}};

    lua_getglobal(L, "outputs");
    if (lua_type(L, -1) != LUA_TTABLE)
    {
        return number_of_outputs;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    for (int i = 0; i < max; i++)
    {
        lua_rawgeti(L, -1, i + 1);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            break;
        }
        luaL_checktype(L, -1, LUA_TTABLE);

        for (int field_index = 0; field_index < 2; field_index++)
        {
            lua_getfield(L, -1, fields[field_index].key);
            luaL_checktype(L, -1, fields[field_index].type);
            switch (field_index)
            {
            case 0:
            {
                output_list[i].tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG__
                fprintf(stderr, "  [OUTPUT] tag: %s, ", output_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                number_of_outputs++;
                output_list[i].uri = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG__
                fprintf(stderr, "uri: %s\n", output_list[i].uri);
#endif
            }
            break;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return number_of_outputs;
} /*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_responder_config(lua_State *L, RESPONDER_CONFIG responder_list[], int max)
{
#ifdef __DEBUG__
    fprintf(stderr, "%s:%i\n", __FUNCTION__, __LINE__);
#endif
    int number_of_responders = 0;
    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        {.key = "tag", .type = LUA_TSTRING},
        {.key = "param", .type = LUA_TSTRING}};

    lua_getglobal(L, "responders");
    if (lua_type(L, -1) != LUA_TTABLE)
    {
        return number_of_responders;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    for (int i = 0; i < max; i++)
    {
        lua_rawgeti(L, -1, i + 1);
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            break;
        }
        luaL_checktype(L, -1, LUA_TTABLE);

        for (int field_index = 0; field_index < 2; field_index++)
        {
            lua_getfield(L, -1, fields[field_index].key);
            luaL_checktype(L, -1, fields[field_index].type);
            switch (field_index)
            {
            case 0:
            {
                responder_list[i].tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG__
                fprintf(stderr, "  [RESPONDER] tag: %s, ", responder_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                number_of_responders++;
                responder_list[i].param = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG__
                fprintf(stderr, "param: %s\n", responder_list[i].param);
#endif
            }
            break;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return number_of_responders;
}

/*
 * ---------------------------------------------------------------------------------------
 */

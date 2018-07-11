
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

#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/luajit.h>

#include "config.h"
#include "dragonfly-cmds.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void unload_inputs_config(INPUT_CONFIG input_list[], int max)
{
    for (int i = 0; i < max; i++)
    {
        if (input_list[i].tag)
        {
            free(input_list[i].tag);
            input_list[i].tag=NULL;
            free(input_list[i].uri);
            input_list[i].uri=NULL;
            free(input_list[i].script);
            input_list[i].script=NULL;
        }
    }
}
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
#ifdef __DEBUG3__
                fprintf(stderr, "  [INPUTS] tag: %s, ", input_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                input_list[i].uri = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG3__
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
                    snprintf(script_path, PATH_MAX, "%s/%s", FILTER_DIR, path);
                }
                else
                {
                    strncpy(script_path, path, PATH_MAX);
                }
                if ((lstat(script_path, &sb) >= 0) && S_ISREG(sb.st_mode))
                {
                    number_of_inputs++;
                    input_list[i].script = strndup(script_path, PATH_MAX);
#ifdef __DEBUG3__
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
void unload_analyzers_config(ANALYZER_CONFIG analyzer_list[], int max)
{
    for (int i = 0; i < max; i++)
    {
        if (analyzer_list[i].tag)
        {
            free(analyzer_list[i].tag);
            analyzer_list[i].tag=NULL;
            free(analyzer_list[i].script);
            analyzer_list[i].script=NULL;
        }
    }
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
#ifdef __DEBUG3__
                fprintf(stderr, "  [ANALYZER] tag: %s, ", analyzer_list[i - 1].tag);
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
                    snprintf(lua_analyzer, PATH_MAX, "%s/%s", ANALYZER_DIR, script_path);
                }
                else
                {
                    strncpy(lua_analyzer, script_path, PATH_MAX);
                }
                if ((lstat(lua_analyzer, &sb) >= 0) && S_ISREG(sb.st_mode))
                {
                    analyzer_list[i - 1].script = strndup(lua_analyzer, PATH_MAX);
#ifdef __DEBUG3__
                    fprintf(stderr, "script: %s\n", analyzer_list[i - 1].script);
#endif
                    number_of_analyzers++;
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
void unload_outputs_config(OUTPUT_CONFIG output_list[], int max)
{
    for (int i = 0; i < max; i++)
    {
        if (output_list[i].tag)
        {
            free(output_list[i].tag);
            output_list[i].tag=NULL;
            free(output_list[i].uri);
            output_list[i].uri=NULL;
        }
    }
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
#ifdef __DEBUG3__
                fprintf(stderr, "  [OUTPUT] tag: %s, ", output_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                number_of_outputs++;
                output_list[i].uri = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG3__
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
} 
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void unload_responder_config(RESPONDER_CONFIG responder_list[], int max)
{
    for (int i = 0; i < max; i++)
    {
        if (responder_list[i].tag)
        {
            free(responder_list[i].tag);
            responder_list[i].tag=NULL;
            free(responder_list[i].param);
            responder_list[i].param=NULL;
        }
    }
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_responder_config(lua_State *L, RESPONDER_CONFIG responder_list[], int max)
{
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
#ifdef __DEBUG3__
                fprintf(stderr, "  [RESPONDER] tag: %s, ", responder_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                number_of_responders++;
                responder_list[i].param = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG3__
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

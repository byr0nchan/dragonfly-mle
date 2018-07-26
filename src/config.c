
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

#include <hiredis/hiredis.h>

#include "param.h"
#include "config.h"
#include "dragonfly-cmds.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

static int load_redis_modules(lua_State *L, redisContext *pContext)
{
    int number_loaded = 0;
    char *tag = NULL;
    char *path = NULL;
    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        {.key = "tag", .type = LUA_TSTRING},
        {.key = "module", .type = LUA_TSTRING}};

    lua_getglobal(L, "modules");
    if (lua_type(L, -1) != LUA_TTABLE)
    {
#ifdef __DEBUG3__
        fprintf(stderr, "%s: no redis modules listed\n", __FUNCTION__);
#endif
        syslog(LOG_INFO, "%s: no redis modules listed\n", __FUNCTION__);
        return 0;
    }
    
    luaL_checktype(L, -1, LUA_TTABLE);
    for (int i = 0; i < MAX_REDIS_MODULES; i++)
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
                tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG3__
                fprintf(stderr, "%s: tag= %s, ", __FUNCTION__, input_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                path = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG3__
                fprintf(stderr, "module=%s, ", __FUNCTION__, input_list[i].uri);
#endif
            }
            break;
            }
            if (tag && path)
            {
                redisReply *reply = redisCommand(pContext, "MODULE LOAD %s", path);
                if (reply && (reply->type != REDIS_REPLY_ERROR))
                {
#ifdef __DEBUG3__
                    fprintf(stderr, "%s: %s\n", __FUNCTION__, path);
#endif
                    syslog(LOG_INFO, "%s: %s\n", __FUNCTION__, path);
                }
                else
                {
                    fprintf(stderr, "%s: failed to load module %s\n", __FUNCTION__, path);
                    syslog(LOG_ERR, "%s: failed to load module %s", __FUNCTION__, path);
                    /* code */
                }
                freeReplyObject(reply);
                tag = NULL;
                path = NULL;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    return number_loaded;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int load_redis(lua_State *L, const char *host, int port)
{
    redisContext *pContext = redisConnect(host, port);
    if (!pContext || pContext->err)
    {
        if (pContext)
        {
            fprintf(stderr, "redisConnect() failed - %s", pContext->errstr);
            syslog(LOG_ERR, "redisConnect() failed - %s", pContext->errstr);
        }
        else
        {
            fprintf(stderr, "redisConnect() failed - unable to connect to server");
            syslog(LOG_ERR, "redisConnect() failed - unable to connect to server");
        }
        return -1;
    }
    int status = load_redis_modules(L, pContext);
    if (!pContext)
    {
        return -1;
    }
    redisFree(pContext);
    return status;
}
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
            input_list[i].tag = NULL;
            free(input_list[i].uri);
            input_list[i].uri = NULL;
            free(input_list[i].script);
            input_list[i].script = NULL;
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
            analyzer_list[i].tag = NULL;
            free(analyzer_list[i].script);
            analyzer_list[i].script = NULL;
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
            output_list[i].tag = NULL;
            free(output_list[i].uri);
            output_list[i].uri = NULL;
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
    char buffer[PATH_MAX];

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

    /* setup internal default output log */
    snprintf(buffer, PATH_MAX, "file://%s", DRAGONFLY_DEFAULT_LOG);
    output_list[DRAGONFLY_LOG_INDEX].tag = strdup(DRAGONFLY_LOG_TAG);
    output_list[DRAGONFLY_LOG_INDEX].uri = strdup(buffer);
    number_of_outputs++;
    /* setup internal default stats output log */
    snprintf(buffer, PATH_MAX, "file://%s", DRAGONFLY_STATS_LOG);
    output_list[DRAGONFLY_STATS_INDEX].tag = strdup(DRAGONFLY_STATS_TAG);
    output_list[DRAGONFLY_STATS_INDEX].uri = strdup(buffer);
    number_of_outputs++;

    int j = 2;
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
                output_list[j].tag = strdup(lua_tostring(L, -1));
#ifdef __DEBUG3__
                fprintf(stderr, "  [OUTPUT] tag: %s, ", output_list[i].tag);
#endif
            }
            break;
            case 1:
            {
                number_of_outputs++;
                output_list[j].uri = strndup(lua_tostring(L, -1), PATH_MAX);
#ifdef __DEBUG3__
                fprintf(stderr, "uri: %s\n", output_list[i].uri);
#endif
            }
            break;
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        j++;
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
            responder_list[i].tag = NULL;
            free(responder_list[i].param);
            responder_list[i].param = NULL;
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

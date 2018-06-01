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

#include <pwd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/luajit.h>

#include "lua-hiredis.h"
#include "lua-cjson.h"
#include "pipe.h"

#include "worker-threads.h"
#include "dragonfly-cmds.h"
#include "dragonfly-io.h"
#include "responder.h"
#include "config.h"
#include "param.h"

extern int g_verbose;
extern int g_chroot;
extern int g_drop_priv;

static char g_root_dir[PATH_MAX];
static char g_run_dir[PATH_MAX];
static char g_log_dir[PATH_MAX];
static char g_scripts_dir[PATH_MAX];
static char g_config_file[PATH_MAX];

extern uint64_t g_running;

static int g_num_analyzer_threads = 0;
static int g_num_input_threads = 0;
static int g_num_output_threads = 0;

static pthread_barrier_t g_barrier;

char *g_redis_host = NULL;
int g_redis_port = 6379;

#define ROTATE_MESSAGE "+rotate+"

typedef struct __DATA_BUFFER_
{
    char buffer[_MAX_BUFFER_SIZE_];
    int len;
    int id;
} DATA_BUFFER;

typedef struct _BUFFER_QUEUE_
{
    DATA_BUFFER list[MAX_DATA_BLOCKS];
    pipe_t *pipe;
    int number;
} BUFFER_QUEUE;

static BUFFER_QUEUE g_buffer_queue;
static INPUT_CONFIG g_input_list[MAX_INPUT_STREAMS];
static INPUT_CONFIG g_flywheel_list[MAX_INPUT_STREAMS];
static OUTPUT_CONFIG g_output_list[MAX_OUTPUT_STREAMS];
static RESPONDER_CONFIG g_responder_list[MAX_RESPONDER_COMMANDS];
static ANALYZER_CONFIG g_analyzer_list[MAX_ANALYZER_STREAMS];

static pthread_t g_thread[(MAX_INPUT_STREAMS * 2) + MAX_OUTPUT_STREAMS + MAX_ANALYZER_STREAMS];

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void signal_abort(int signum)
{
    g_running = 0;
    syslog(LOG_ERR, "%s", __FUNCTION__);
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void lua_disable_io(lua_State *L)
{
    /*
     * Disable I/O in the loop() entry point. Reduces security risk.
     */
    lua_pushnil(L);
    lua_setglobal(L, "io");
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void signal_shutdown(int result)
{
    g_running = 0;
#ifdef __DEBUG3__
    fprintf(stderr, "%s: shutting down.\n", __FUNCTION__);
#endif
    syslog(LOG_INFO, "shutting down");
    exit(result);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void signal_log_rotate(int signum)
{
    syslog(LOG_INFO, "%s", __FUNCTION__);
    for (int i = 0; g_output_list[i].tag != NULL; i++)
    {
        DATA_BUFFER *ptr = pipe_pop(g_buffer_queue.pipe);
        ptr->len = strnlen(ROTATE_MESSAGE, _MAX_BUFFER_SIZE_);
        strcpy(ptr->buffer, ROTATE_MESSAGE);
        pipe_push(g_output_list[i].pipe, (void *)ptr);
    }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int analyze_event(lua_State *L)
{
    if (lua_gettop(L) != 2)
    {
        return luaL_error(L, "expecting exactly 2 arguments");
    }
    size_t len = 0;
    const char *name = luaL_checkstring(L, 1);
    const char *message = lua_tolstring(L, 2, &len);
    for (int i = 0; g_analyzer_list[i].tag != NULL; i++)
    {
        if (strcasecmp(name, g_analyzer_list[i].tag) == 0)
        {
            DATA_BUFFER *ptr = pipe_pop(g_buffer_queue.pipe);
            memcpy(ptr->buffer, message, len);
            ptr->len = len;
            ptr->buffer[len] = '\0';
            pipe_push(g_analyzer_list[i].pipe, (void *)ptr);
            return 0;
        }
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int output_event(lua_State *L)
{
    if (lua_gettop(L) != 2)
    {
        return luaL_error(L, "expecting exactly 2 arguments");
    }
    size_t len = 0;
    const char *name = luaL_checkstring(L, 1);
    const char *message = lua_tolstring(L, 2, &len);

    for (int i = 0; g_output_list[i].tag != NULL; i++)
    {
        if (strcasecmp(name, g_output_list[i].tag) == 0)
        {
            DATA_BUFFER *ptr = pipe_pop(g_buffer_queue.pipe);
            ptr->len = len;
            memcpy(ptr->buffer, message, ptr->len);
            ptr->buffer[ptr->len] = '\0';
            pipe_push(g_output_list[i].pipe, (void *)ptr);
            return 0;
        }
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int response_event(lua_State *L)
{
    if (lua_gettop(L) != 2)
    {
        return luaL_error(L, "expecting exactly 2 arguments");
    }

    const char *tag = luaL_checkstring(L, 1);
    const char *command = luaL_checkstring(L, 2);
    char response[2048];

    if (responder_event(tag, command, response, sizeof(response)) < 0)
    {
        lua_pushnil(L);
    }
    else
    {
        lua_pushstring(L, response);
    }
    return 1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void lua_flywheel_loop(INPUT_CONFIG *flywheel)
{
    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(g_buffer_queue.pipe, (void **)ptr, MAX_IO_VECTOR);
        for (int i = 0; i < n; i++)
        {
            if ((ptr[i]->len = dragonfly_io_read(flywheel->input, ptr[i]->buffer, (_MAX_BUFFER_SIZE_ - 1))) < 0)
            {
                fprintf(stderr, "DEBUG-> %s: %i read ERROR\n", __FUNCTION__, __LINE__);
                pipe_push(g_buffer_queue.pipe, (void *)ptr);
                return;
            }
            ptr[i]->buffer[ptr[i]->len] = '\0';
            pipe_push(flywheel->pipe, (void *)ptr[i]);
        }
    }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void *lua_flywheel_thread(void *ptr)
{
    INPUT_CONFIG *flywheel = (INPUT_CONFIG *)ptr;

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), flywheel->tag);
    pthread_barrier_wait(&g_barrier);

    syslog(LOG_NOTICE, "Running %s\n", flywheel->tag);
    while (g_running)
    {
        if ((flywheel->input = dragonfly_io_open(flywheel->uri, DF_IN)) == NULL)
        {
            break;
        }
        lua_flywheel_loop(flywheel);
        dragonfly_io_close(flywheel->input);
    }

    syslog(LOG_NOTICE, "%s exiting", flywheel->tag);
    pthread_exit(NULL);
    return (void *)NULL;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void lua_input_loop(lua_State *L, INPUT_CONFIG *input)
{
    /*
     * Disable I/O in the loop() entry point.
     */
    lua_disable_io(L);

    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(input->pipe, (void **)ptr, MAX_IO_VECTOR);
        for (int i = 0; i < n; i++)
        {
            lua_getglobal(L, "loop");
            lua_pushlstring(L, ptr[i]->buffer, ptr[i]->len);
            int error = lua_pcall(L, 1, 0, 0);
            if (error)
            {
                syslog(LOG_ERR, "%s: lua_pcall error (%d): - %s", __FUNCTION__, error, lua_tostring(L, -1));
                lua_pop(L, 1);
                exit(EXIT_FAILURE);
            }
            pipe_push(g_buffer_queue.pipe, (void *)ptr[i]);
        }
    }
    exit(1);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void *lua_input_thread(void *ptr)
{
    INPUT_CONFIG *input = (INPUT_CONFIG *)ptr;
    char lua_path[PATH_MAX];
    char *lua_script = input->script;

#ifdef __DEBUG3__
    fprintf(stderr, "%s: started thread %s\n", __FUNCTION__, input->tag);
#endif

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), input->tag);

    /*
     * Set thread name to the file name of the lua script
     */
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    /* set local LUA paths */
    snprintf(lua_path, PATH_MAX - 1, "package.path = package.path .. \";lib/?.lua\"");
    if (luaL_dostring(L, lua_path))
    {
        syslog(LOG_ERR, "luaL_dostring %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }

    snprintf(lua_path, PATH_MAX - 1, "package.cpath = package.cpath .. \";lib/?.so\"");
    if (luaL_dostring(L, lua_path))
    {
        syslog(LOG_ERR, "luaL_dostring %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }

    /*
     * Load the lua-cjson library:
     * 
     *  https://github.com/mpx/lua-cjson
     * 
     */
    luaopen_cjson(L);
    luaopen_cjson_safe(L);
    if (g_verbose)
    {
        syslog(LOG_INFO, "Loaded lua-cjson library");
        fprintf(stderr, "%s: loaded lua-cjson library\n", __FUNCTION__);
    }
    /*
     * Load the lua-hiredis library:
     * 
     *  https://github.com/agladysh/lua-hiredis.git
     * 
     */
    luaopen_hiredis(L, g_redis_host, g_redis_port);
    if (g_verbose)
    {
        syslog(LOG_INFO, "loaded lua-hiredis library");
        fprintf(stderr, "%s: loaded lua-hiredis library\n", __FUNCTION__);
    }

    if (luaL_loadfile(L, lua_script) || (lua_pcall(L, 0, 0, 0) == LUA_ERRRUN))
    {
        syslog(LOG_ERR, "luaL_loadfile %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON);
    syslog(LOG_INFO, "Loaded %s", lua_script);

    /* register functions */
    lua_pushcfunction(L, analyze_event);
    lua_setglobal(L, "analyze_event");

    /* initialize the script */
    lua_getglobal(L, "setup");
    if (lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "%s error; %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        signal_shutdown(-1);
    }

    pthread_barrier_wait(&g_barrier);
    syslog(LOG_NOTICE, "Running %s\n", lua_script);

    while (g_running)
    {
        lua_input_loop(L, input);
    }

    lua_close(L);
    syslog(LOG_NOTICE, "%s exiting", lua_script);
    pthread_exit(NULL);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void lua_output_loop(OUTPUT_CONFIG *output)
{

    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(output->pipe, (void **)ptr, MAX_IO_VECTOR);

        for (int i = 0; i < n; i++)
        {
            if (strcasecmp(ptr[i]->buffer, ROTATE_MESSAGE) == 0)
            {
                dragonfly_io_rotate(output->output);
            }
            else
            {
                if (dragonfly_io_write(output->output, ptr[i]->buffer) < 0)
                {
                    fprintf(stderr, "%s: output error\n", __FUNCTION__);
                    pipe_push(g_buffer_queue.pipe, (void *)ptr);
                    return;
                }
            }
            pipe_push(g_buffer_queue.pipe, (void *)ptr[i]);
        }
    }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void *lua_output_thread(void *ptr)
{
    OUTPUT_CONFIG *output = (OUTPUT_CONFIG *)ptr;

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), output->tag);

    pthread_barrier_wait(&g_barrier);
    syslog(LOG_NOTICE, "Running %s\n", output->tag);

    while (g_running)
    {
        if ((output->output = dragonfly_io_open(output->uri, DF_OUT)) == NULL)
        {
            break;
        }
        lua_output_loop(output);
        dragonfly_io_close(output->output);
    }

    syslog(LOG_NOTICE, "%s exiting", output->tag);
    pthread_exit(NULL);

    return (void *)NULL;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void lua_analyzer_loop(lua_State *L, ANALYZER_CONFIG *analyzer)
{
    /*
     * Disable I/O in the loop() entry point.
     */
    lua_disable_io(L);

    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(analyzer->pipe, (void **)ptr, MAX_IO_VECTOR);
        for (int i = 0; i < n; i++)
        {
            lua_getglobal(L, "loop");
            lua_pushlstring(L, ptr[i]->buffer, ptr[i]->len);
            if (lua_pcall(L, 1, 0, 0))
            {
                syslog(LOG_ERR, "lua_pcall error: %s - %s", __FUNCTION__, lua_tostring(L, -1));
                lua_pop(L, 1);
                exit(EXIT_FAILURE);
            }
        }
        pipe_pushv(g_buffer_queue.pipe, (void **)ptr, n);
    }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void *lua_analyzer_thread(void *ptr)
{
    ANALYZER_CONFIG *analyzer = (ANALYZER_CONFIG *)ptr;
    char lua_path[PATH_MAX];
    char *lua_script = analyzer->script;

#ifdef __DEBUG3__
    fprintf(stderr, "%s: started thread %s\n", __FUNCTION__, analyzer->tag);
#endif

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), analyzer->tag);
    /*
     * Set thread name to the file name of the lua script
     */
    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    /* set local LUA paths */
    snprintf(lua_path, PATH_MAX - 1, "package.path = package.path .. \";lib/?.lua\"");
    if (luaL_dostring(L, lua_path))
    {
        syslog(LOG_ERR, "luaL_dostring %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }

    snprintf(lua_path, PATH_MAX - 1, "package.cpath = package.cpath .. \";lib/?.so\"");
    if (luaL_dostring(L, lua_path))
    {
        syslog(LOG_ERR, "luaL_dostring %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }

    if (luaL_loadfile(L, lua_script) || (lua_pcall(L, 0, 0, 0) == LUA_ERRRUN))
    {
        syslog(LOG_ERR, "luaL_loadfile %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON);
    syslog(LOG_INFO, "Loaded %s", lua_script);

    /*
     * Load the lua-cjson library:
     * 
     *  https://github.com/mpx/lua-cjson
     * 
     */
    luaopen_cjson(L);
    luaopen_cjson_safe(L);
    if (g_verbose)
    {
        syslog(LOG_INFO, "Loaded lua-cjson library");
        fprintf(stderr, "%s: loaded lua-cjson library\n", __FUNCTION__);
    }
    /*
     * Load the lua-hiredis library:
     * 
     *  https://github.com/agladysh/lua-hiredis.git
     * 
     */
    luaopen_hiredis(L, g_redis_host, g_redis_port);
    if (g_verbose)
    {
        syslog(LOG_INFO, "loaded lua-hiredis library");
        fprintf(stderr, "%s: loaded lua-hiredis library\n", __FUNCTION__);
    }
    /* register functions */
    lua_pushcfunction(L, output_event);
    lua_setglobal(L, "output_event");

    /*
     * Initialize responders commands;
     */
    responder_initialize();

    for (int i = 0; i < MAX_RESPONDER_COMMANDS; i++)
    {
        if (g_responder_list[i].tag && g_responder_list[i].param)
        {
            if (responder_setup(g_responder_list[i].tag, g_responder_list[i].param) < 0)
            {
                syslog(LOG_ERR, "responder_setup %s failed", g_responder_list[i].tag);
#ifdef __DEBUG3__
                fprintf(stderr, "responder_setup %s failed\n", g_responder_list[i].tag);
#endif
                signal_shutdown(-1);
            }
        }
    }

    lua_pushcfunction(L, response_event);
    lua_setglobal(L, "response_event");

    lua_pushcfunction(L, dragonfly_http_get);
    lua_setglobal(L, "http_get");

#ifdef __DATE_FUNCTION__
    lua_pushcfunction(L, dragonfly_date2epoch);
    lua_setglobal(L, "date2epoch");
#endif

    /* initialize the script */
    lua_getglobal(L, "setup");
    if (lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "lua_pcall error: %s - %s", __FUNCTION__, lua_tostring(L, -1));
        lua_pop(L, 1);
        signal_shutdown(-1);
    }

    pthread_barrier_wait(&g_barrier);
    syslog(LOG_NOTICE, "Running %s\n", lua_script);

    while (g_running)
    {
        lua_analyzer_loop(L, analyzer);
    }
    lua_close(L);

    syslog(LOG_NOTICE, "%s exiting", lua_script);
    pthread_exit(NULL);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void destroy_configuration()
{
    unload_inputs_config(g_input_list, MAX_INPUT_STREAMS);
    unload_outputs_config(g_output_list, MAX_OUTPUT_STREAMS);
    unload_analyzers_config(g_analyzer_list, MAX_ANALYZER_STREAMS);

    g_num_analyzer_threads = 0;
    g_num_input_threads = 0;
    g_num_output_threads = 0;
    memset(g_analyzer_list, 0, sizeof(g_analyzer_list));
    pipe_free(g_buffer_queue.pipe);
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void initialize_configuration(const char *dragonfly_root)
{
    g_running = 1;
    strncpy(g_root_dir, dragonfly_root, PATH_MAX);
    if (chdir(g_root_dir) != 0)
    {
        syslog(LOG_ERR, "unable to chdir() to  %s", g_root_dir);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "root dir: %s\n", g_root_dir);
    if (g_chroot)
    {
        if (chroot(g_root_dir) != 0)
        {
            syslog(LOG_ERR, "unable to chroot() to : %s - %s\n", g_root_dir, strerror(errno));
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "chroot: %s\n", g_root_dir);
    }
    char *path = get_current_dir_name();
    syslog(LOG_INFO, "chdir: %s\n", path);
    free(path);

#ifdef __DEBUG__
    snprintf(g_run_dir, PATH_MAX, "%s/%s", dragonfly_root, RUN_DIR);
    snprintf(g_scripts_dir, PATH_MAX, "%s/%s", dragonfly_root, SCRIPTS_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_scripts_dir, CONFIG_FILE);
#endif

    strncpy(g_run_dir, RUN_DIR, PATH_MAX);
    strncpy(g_scripts_dir, SCRIPTS_DIR, PATH_MAX);
    snprintf(g_log_dir, PATH_MAX, "%s", LOG_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_scripts_dir, CONFIG_FILE);

    syslog(LOG_INFO, "run dir: %s\n", g_run_dir);
    syslog(LOG_INFO, "analyzer dir: %s\n", g_scripts_dir);

    struct stat sb;
    if ((lstat(g_config_file, &sb) < 0) || !S_ISREG(sb.st_mode))
    {
        fprintf(stderr, "%s does not exist.\n", g_config_file);
        syslog(LOG_WARNING, "%s does noet exist.\n", g_config_file);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "config file: %s\n", g_config_file);

    g_num_analyzer_threads = 0;
    g_num_input_threads = 0;
    g_num_output_threads = 0;
    memset(g_analyzer_list, 0, sizeof(g_analyzer_list));
    memset(g_input_list, 0, sizeof(g_input_list));
    memset(g_output_list, 0, sizeof(g_output_list));
    /*
     * prime free list
    */
    g_buffer_queue.pipe = pipe_new(MAX_DATA_BLOCKS);

    for (int i = 0; i < MAX_DATA_BLOCKS; i++)
    {
        DATA_BUFFER *ptr = &g_buffer_queue.list[i];
        ptr->id = i;
        pipe_push(g_buffer_queue.pipe, ptr);
    }
    lua_State *L = luaL_newstate();
    /*
     * Load config.lua
     */
    if (luaL_loadfile(L, g_config_file))
    {
        syslog(LOG_ERR, "luaL_loadfile failed; %s", lua_tostring(L, -1));
        abort();
    }
    if (lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "lua_pcall error: %s - %s", __FUNCTION__, lua_tostring(L, -1));
        lua_pop(L, 1);
        abort();
    }
    lua_getglobal(L, "redis_port");
    if (lua_isstring(L, -1))
    {
        g_redis_port = atoi(lua_tostring(L, -1));
    }
    lua_getglobal(L, "redis_host");
    if (lua_isstring(L, -1))
    {
        g_redis_host = strdup(lua_tostring(L, -1));
    }
    else
    {
        g_redis_host = strdup("127.0.0.1");
    }

    if ((g_num_analyzer_threads = load_analyzers_config(L, g_analyzer_list, MAX_ANALYZER_STREAMS)) <= 0)
    {
        syslog(LOG_ERR, "load_analyzer_config failed");
        abort();
    }
    if ((g_num_output_threads = load_outputs_config(L, g_output_list, MAX_OUTPUT_STREAMS)) <= 0)
    {
        syslog(LOG_ERR, "load_output_config failed");
        abort();
    }
    if ((g_num_input_threads = load_inputs_config(L, g_input_list, MAX_INPUT_STREAMS)) <= 0)
    {
        syslog(LOG_ERR, "load_input_config failed");
        abort();
    }
    if ((load_responder_config(L, g_responder_list, MAX_RESPONDER_COMMANDS)) < 0)
    {
        syslog(LOG_ERR, "load_responder_config failed");
        abort();
    }
    lua_close(L);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void shutdown_threads()
{
    g_running = 0;
    sleep (1);

    int n = 0;
    while (g_thread[n])
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s:%i %i\n", __FUNCTION__, __LINE__, n);
#endif
        pthread_join(g_thread[n++], NULL);
    }

    for (int i = 0; g_input_list[i].uri != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_input_list[i].script);
#endif
        pipe_free(g_input_list[i].pipe);
    }
    for (int i = 0; g_analyzer_list[i].script != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_analyzer_list[i].script);
#endif
        pipe_free(g_analyzer_list[i].pipe);
    }
    for (int i = 0; g_output_list[i].pipe != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_analyzer_list[i].script);
#endif
        pipe_free(g_output_list[i].pipe);
    }
    destroy_configuration();

    pthread_barrier_destroy(&g_barrier);

    g_num_analyzer_threads = 0;
    g_num_input_threads = 0;
    g_num_output_threads = 0;
    free(g_redis_host);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void startup_threads(const char *dragonfly_root)
{
    signal(SIGUSR1, signal_log_rotate);
    signal(SIGABRT, signal_abort);
    pthread_barrier_init(&g_barrier, NULL,
                         (g_num_analyzer_threads + (g_num_input_threads * 2) + g_num_output_threads + 1));
    initialize_configuration(dragonfly_root);
#ifdef __DEBUG3__
    fprintf(stderr, "%s\n", __FUNCTION__);
#endif
    /*
     * Caution: The path below must be defined relative to chdir (g_run_dir) 
     *		so that it works if chroot() is in effect. See above.
     */
    int n = 0;
    memset(g_thread, 0, sizeof(g_thread));
    for (int i = 0; i < MAX_INPUT_STREAMS; i++)
    {
        if (g_input_list[i].uri != NULL)
        {
            for (int j = 0; j < MAX_WORKER_THREADS; j++)
            {
                g_input_list[i].pipe = pipe_new(MAX_PIPE_LENGTH);
                /*
         * check that file exists with execute permissions
         */
                if (pthread_create(&(g_thread[n++]), NULL, lua_input_thread, (void *)&g_input_list[i]) != 0)
                {
                    syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
                    pthread_exit(NULL);
                }
            }
        }
    }
    // make a copy
    memcpy(g_flywheel_list, g_input_list, sizeof(g_flywheel_list));

    for (int i = 0; i < MAX_INPUT_STREAMS; i++)
    {
        if (g_flywheel_list[i].uri != NULL)
        {
            /*
         * check that file exists with execute permissions
         */

            if (pthread_create(&(g_thread[n++]), NULL, lua_flywheel_thread, (void *)&g_flywheel_list[i]) != 0)
            {
                syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
                pthread_exit(NULL);
            }
        }
    }

    for (int i = 0; i < MAX_OUTPUT_STREAMS; i++)
    {
        if (g_output_list[i].uri != NULL)
        {
            g_output_list[i].pipe = pipe_new(MAX_PIPE_LENGTH);
            /*
         * check that file exists with execute permissions
         */
            for (int j = 0; j < MAX_WORKER_THREADS; j++)
            {

                if (pthread_create(&(g_thread[n++]), NULL, lua_output_thread, (void *)&g_output_list[i]) != 0)
                {
                    syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
                    pthread_exit(NULL);
                }
            }
        }
    }

    for (int i = 0; i < MAX_ANALYZER_STREAMS; i++)
    {
        if (g_analyzer_list[i].script != NULL)
        {
            g_analyzer_list[i].pipe = pipe_new(MAX_PIPE_LENGTH);

            /*
         * check that file exists with execute permissions
         */
            for (int j = 0; j < MAX_WORKER_THREADS; j++)
            {
                if (pthread_create(&(g_thread[n++]), NULL, lua_analyzer_thread, (void *)&g_analyzer_list[i]) != 0)
                {
                    syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
                    pthread_exit(NULL);
                }
            }
        }
    }

    /*
     * Wait until all analyzer threads are ready
     */
#ifdef __DEBUG3__
    fprintf(stderr, "%s:%i pthread_barrier_wait()\n", __FUNCTION__, __LINE__);
#endif
    pthread_barrier_wait(&g_barrier);

    //if (getuid() == 0)
    if (g_drop_priv)
    {
        fprintf(stderr, "\nDropping privileges\n");
        if (setgid(getgid()) < 0)
        {
                syslog(LOG_ERR, "setgid: %s", strerror(errno));
        }
        struct passwd *pwd = getpwnam(USER_NOBODY);
        if (pwd && setuid(pwd->pw_uid) != 0)
        //if (setuid(getuid()) <0)
        {
            syslog(LOG_ERR, "setuid(%s): %s", USER_NOBODY, strerror(errno));
            signal_shutdown(-1);
        }
        syslog(LOG_INFO, "dropped privileges: %s\n", USER_NOBODY);
    }
    syslog(LOG_INFO, "%s: threads running\n", __FUNCTION__);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void launch_lua_threads(const char *root_directory)
{
    g_verbose = isatty(1);
    startup_threads(root_directory);
}

/*
 * ---------------------------------------------------------------------------------------
 */

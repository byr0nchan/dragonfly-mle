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

#include <lauxlib.h>
#include <luajit-2.0/luajit.h>

#include "lua-hiredis.h"
#include "lua-cjson.h"
#include "pipe.h"

#include "worker-threads.h"
#include "dragonfly-cmds.h"
#include "dragonfly-io.h"
#include "config.h"
#include "param.h"

extern int g_verbose;
extern int g_chroot;
extern int g_drop_priv;

static char g_root_dir[PATH_MAX];
static char g_run_dir[PATH_MAX];
static char g_log_dir[PATH_MAX];
static char g_analyzer_dir[PATH_MAX];
static char g_config_file[PATH_MAX];

extern uint64_t g_running;

static int g_num_analyzer_threads = 0;
static int g_num_input_threads = 0;
static int g_num_output_threads = 0;

static pthread_barrier_t g_barrier;

char *g_redis_host = "127.0.0.1";
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
} BUFFER_QUEUE;

static BUFFER_QUEUE g_buffer_queue;
static INPUT_CONFIG g_input_list[MAX_INPUT_STREAMS];
static INPUT_CONFIG g_flywheel_list[MAX_INPUT_STREAMS];
static OUTPUT_CONFIG g_output_list[MAX_OUTPUT_STREAMS];
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
void signal_shutdown(int result)
{
    g_running = 0;
    syslog(LOG_INFO, "Shutting down");
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
        pipe_push(g_output_list[i].pipe, ptr);
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
    const char *name = luaL_checkstring(L, 1);
    const char *message = luaL_checkstring(L, 2);
    for (int i = 0; g_analyzer_list[i].tag != NULL; i++)
    {
        if (strcasecmp(name, g_analyzer_list[i].tag) == 0)
        {

            DATA_BUFFER *ptr = pipe_pop(g_buffer_queue.pipe);
            //ptr->len = strnlen(message, _MAX_BUFFER_SIZE_);
            strcpy(ptr->buffer, message);
            pipe_push(g_analyzer_list[i].pipe, ptr);
            return 0;
        }
    }
    return -1;
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

    const char *name = luaL_checkstring(L, 1);
    const char *message = luaL_checkstring(L, 2);

    for (int i = 0; g_output_list[i].tag != NULL; i++)
    {
        if (strcasecmp(name, g_output_list[i].tag) == 0)
        {
            DATA_BUFFER *ptr = pipe_pop(g_buffer_queue.pipe);
            ptr->len = strnlen(message, _MAX_BUFFER_SIZE_);
            strcpy(ptr->buffer, message);
            pipe_push(g_output_list[i].pipe, ptr);
            return 0;
        }
    }
    return -1;
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
                pipe_push(g_buffer_queue.pipe, ptr);
                return;
            }
            if (ptr[i]->len)
            {
                ptr[i]->buffer[ptr[i]->len] = '\0';
                pipe_push(flywheel->pipe, ptr[i]);
            }
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
    // pthread_setname_np(pthread_self(), flywheel->name);
    pthread_setname_np(pthread_self(), "flywheel");
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
    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(input->pipe, (void **)ptr, MAX_IO_VECTOR);
        for (int i = 0; i < n; i++)
        {
            ptr[i]->buffer[ptr[i]->len] = '\0';
            lua_getglobal(L, "loop");
            lua_pushstring(L, ptr[i]->buffer);
            if (lua_pcall(L, 1, 0, 0))
            {
                syslog(LOG_ERR, "lua_pcall error: %s - %s", __FUNCTION__, lua_tostring(L, -1));
                lua_pop(L, 1);
                exit(EXIT_FAILURE);
            }
            pipe_push(g_buffer_queue.pipe, ptr[i]);
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

#ifdef __DEBUG__
    fprintf(stderr, "%s: started thread %s\n", __FUNCTION__, input->tag);
#endif

    pthread_detach(pthread_self());
    //pthread_setname_np(pthread_self(), input->tag);
    pthread_setname_np(pthread_self(), "input");
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

    if (luaL_loadfile(L, lua_script) || lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "luaL_loadfile %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE|LUAJIT_MODE_ON);
    syslog(LOG_INFO, "Loaded %s", lua_script);

    /*
     * Load the lua-cjson library:
     * 
     *  https://github.com/mpx/lua-cjson
     * 
     */
    luaopen_cjson(L);
    luaopen_cjson_safe(L);
    syslog(LOG_INFO, "Loaded lua-cjson library");
    /*
     * Load the lua-hiredis library:
     * 
     *  https://github.com/agladysh/lua-hiredis.git
     *  
     */
    // Disabled for the [INPUT] stage
    //luaopen_hiredis(L, g_redis_host, g_redis_port);
    //syslog(LOG_INFO, "loaded lua-hiredis library");

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
                if (dragonfly_io_write(output->output, ptr[i]->buffer) <= 0)
                {
                    fprintf(stderr, "DEBUG-> %s pipe_pope ERROR)\n", __FUNCTION__);
                    pipe_push(g_buffer_queue.pipe, ptr);
                    return;
                }
            }
            pipe_push(g_buffer_queue.pipe, ptr[i]);
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
    //thread_setname_np(pthread_self(), output->name);
    pthread_setname_np(pthread_self(), "output");

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
    while (g_running)
    {
        DATA_BUFFER *ptr[MAX_IO_VECTOR];
        int n = pipe_popv(analyzer->pipe, (void **)ptr, MAX_IO_VECTOR);
        for (int i = 0; i < n; i++)
        {
            ptr[i]->buffer[ptr[i]->len] = '\0';
            lua_getglobal(L,"loop");
            lua_pushstring(L, ptr[i]->buffer);
            
            if (lua_pcall(L, 1, 0, 0))
            {
                syslog(LOG_ERR, "lua_pcall error: %s - %s", __FUNCTION__, lua_tostring(L, -1));
                lua_pop(L, 1);
                exit(EXIT_FAILURE);
            }
        }
        pipe_pushv(g_buffer_queue.pipe,(void **) ptr, n);
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

#ifdef __DEBUG__
    fprintf(stderr, "%s: started thread %s\n", __FUNCTION__, analyzer->tag);
#endif

    pthread_detach(pthread_self());
    //pthread_setname_np(pthread_self(), analyzer->tag);
    pthread_setname_np(pthread_self(), "analyzer");
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

    if (luaL_loadfile(L, lua_script) || lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "luaL_loadfile %s failed - %s", lua_script, lua_tostring(L, -1));
        lua_pop(L, 1);
        pthread_exit(NULL);
    }
    luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE|LUAJIT_MODE_ON);
    syslog(LOG_INFO, "Loaded %s", lua_script);

    /*
     * Load the lua-cjson library:
     * 
     *  https://github.com/mpx/lua-cjson
     * 
     */
    luaopen_cjson(L);
    luaopen_cjson_safe(L);
    syslog(LOG_INFO, "Loaded lua-cjson library");
#ifdef __DEBUG__
    fprintf(stderr, "%s: loaded lua-cjson library\n", __FUNCTION__);
#endif
    /*
     * Load the lua-hiredis library:
     * 
     *  https://github.com/agladysh/lua-hiredis.git
     * 
     */
    luaopen_hiredis(L, g_redis_host, g_redis_port);
    syslog(LOG_INFO, "loaded lua-hiredis library");
#ifdef __DEBUG__
    fprintf(stderr, "%s: loaded lua-hiredis library\n", __FUNCTION__);
#endif
    /* register functions */
    lua_pushcfunction(L, output_event);
    lua_setglobal(L, "output_event");

#ifdef __ENABLE_RESPONDERS
    // TODO:
#endif
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
    for (int i = 0; g_input_list[i].script != NULL; i++)
    {
        free(g_input_list[i].uri);
        g_input_list[i].uri = NULL;
        free(g_input_list[i].script);
        g_input_list[i].script = NULL;
    }
    for (int i = 0; g_analyzer_list[i].script != NULL; i++)
    {
        free(g_analyzer_list[i].tag);
        g_analyzer_list[i].tag = NULL;
        free(g_analyzer_list[i].script);
        g_analyzer_list[i].script = NULL;
    }
    for (int i = 0; g_output_list[i].uri != NULL; i++)
    {
        free(g_output_list[i].tag);
        g_output_list[i].tag = NULL;
        free(g_output_list[i].uri);
        g_output_list[i].uri = NULL;
    }

    g_num_analyzer_threads = 0;
    g_num_input_threads = 0;
    g_num_output_threads = 0;
    memset(g_analyzer_list, 0, sizeof(g_analyzer_list));
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
    syslog(LOG_INFO, "chdir: %s\n", getcwd(g_root_dir, PATH_MAX));

#ifdef __DEBUG__
    snprintf(g_run_dir, PATH_MAX, "%s/%s", dragonfly_root, RUN_DIR);
    snprintf(g_analyzer_dir, PATH_MAX, "%s/%s", dragonfly_root, ANALYZERS_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_analyzer_dir, CONFIG_FILE);
#endif

    strncpy(g_run_dir, RUN_DIR, PATH_MAX);
    strncpy(g_analyzer_dir, ANALYZERS_DIR, PATH_MAX);
    snprintf(g_log_dir, PATH_MAX, "%s", LOG_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_analyzer_dir, CONFIG_FILE);

    syslog(LOG_INFO, "run dir: %s\n", g_run_dir);
    syslog(LOG_INFO, "analyzer dir: %s\n", g_analyzer_dir);

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
        //fprintf(stderr, "DEBUG-> %s-%i  buffer(%p : %i)\n", __FUNCTION__, __LINE__, ptr, ptr->id);
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
    // send an interrup signal to all of the threadsjoin()
    int n = 0;
    while (g_thread[n])
    {
        pthread_join(g_thread[n++], NULL);
    }

    for (int i = 0; g_input_list[i].uri != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_input_list[i].script);
#endif
        //pthread_join(g_input_list[i].thread, NULL);
        pipe_free(g_input_list[i].pipe);
    }
    for (int i = 0; g_analyzer_list[i].script != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_analyzer_list[i].script);
#endif
        //pthread_join(g_analyzer_list[i].thread, NULL);
        pipe_free(g_analyzer_list[i].pipe);
    }
    for (int i = 0; g_output_list[i].uri != NULL; i++)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: waiting on %s\n", __FUNCTION__, g_analyzer_list[i].script);
#endif
        //pthread_join(g_output_list[i].thread, NULL);
        pipe_free(g_output_list[i].pipe);
    }
    destroy_configuration();

    pthread_barrier_destroy(&g_barrier);

    g_num_analyzer_threads = 0;
    g_num_input_threads = 0;
    g_num_output_threads = 0;
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
    pthread_barrier_init(&g_barrier, NULL, (g_num_analyzer_threads + (g_num_input_threads * 2) + g_num_output_threads + 1));
    initialize_configuration(dragonfly_root);

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
    sleep(2);
    pthread_barrier_wait(&g_barrier);

    if (g_drop_priv)
    {
        struct passwd *pwd = getpwnam(USER_NOBODY);
        if (getuid() == 0)
        {
            /* process is running as root, drop privileges */
            if (pwd && setuid(pwd->pw_uid) != 0)
            {
                syslog(LOG_ERR, "setuid: unable to drop user privileges: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
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

void launch_lua_analyzers(const char *root_directory)
{
    g_verbose = isatty(1);
    startup_threads(root_directory);
}

/*
 * ---------------------------------------------------------------------------------------
 */

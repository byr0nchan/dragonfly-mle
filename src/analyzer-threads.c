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
 *
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

#include <luajit-2.0/luajit.h>

#include <analyzer-threads.h>
#include <suricata-cmds.h>
#include <dragonfly-cmds.h>
#include <dragonfly-io.h>

extern int g_verbose;
extern int g_chroot;
extern int g_drop_priv;

static char g_root_dir[PATH_MAX];
static char g_run_dir[PATH_MAX];
static char g_log_dir[PATH_MAX];
static char g_analyzer_dir[PATH_MAX];
static char g_config_file[PATH_MAX];

extern uint64_t g_running;

static int g_num_lua_threads = 0;
static pthread_barrier_t g_barrier;

static char *g_redis_host = "127.0.0.1";
static char *g_redis_port = "6379";

static pthread_key_t g_threadContext;

#define MAX_ANALYZERS 16

typedef struct _ANALYZER_CONFIG_
{
    char *name;
    char *input_uri;
    char *script_path;
    char *output_uri;
    DF_HANDLE *input;
    DF_HANDLE *output;
} ANALYZER_CONFIG;

static ANALYZER_CONFIG g_analyzer_list[MAX_ANALYZERS];

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void thread_context_destroy(void *data)
{
    pthread_setspecific(g_threadContext, NULL);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void dragonfly_log_rotate(int signum)
{
    for (int i = 0; i < MAX_ANALYZERS; i++)
    {
        if (g_analyzer_list[i].output)
        {
            dragonfly_io_rotate(g_analyzer_list[i].output);
        }
    }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int dragonfly_log(lua_State *L)
{
    if (lua_gettop(L) != 4)
    {
        return luaL_error(L, "expecting exactly 4 arguments");
    }
    const char *timestamp = luaL_checkstring(L, 1);
    const char *category = luaL_checkstring(L, 2);
    const char *action = luaL_checkstring(L, 3);
    const char *message = luaL_checkstring(L, 4);

    ANALYZER_CONFIG *analyzer = (ANALYZER_CONFIG *)pthread_getspecific(g_threadContext);

    char buffer[1024];
    snprintf(buffer, sizeof(buffer),
             "{\"timestamp\":\"%s\",\"event_type\":\"%s\",\"notice\":{\"category\":\"%s\",\"action\":\"%s\",\"message\":\"%s\"}}\n",
             timestamp, analyzer->name, category, action, message);
    dragonfly_io_write(analyzer->output, buffer);
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void lua_analyzer_loop(lua_State *L, DF_HANDLE *df_input, DF_HANDLE *df_output)
{
    char buffer[2048];
    memset(buffer, 0, sizeof(buffer));

    while (g_running)
    {
        int n = dragonfly_io_read(df_input, buffer, sizeof(buffer) - 1);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            syslog(LOG_ERR, "socket read error: %s", strerror(errno));
            return;
        }
        else if (n == 0)
        {
            syslog(LOG_ERR, "connection closed; reconnecting");
            return;
        }
        lua_getglobal(L, "event");
        lua_pushlstring(L, buffer, n);
        if (lua_pcall(L, 1, 0, 0))
        {
            syslog(LOG_ERR, "lua_pcall error; %s", lua_tostring(L, -1));
        }
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
    char lua_analyzer[PATH_MAX];
    char input_uri[PATH_MAX];
    char output_uri[PATH_MAX];
    char lua_path[PATH_MAX];

    pthread_detach(pthread_self());
    pthread_setname_np(pthread_self(), analyzer->name);
    if (pthread_setspecific(g_threadContext, analyzer) != 0)
    {
        syslog(LOG_ERR, "pthread_setspecific error; %s", strerror(errno));
        pthread_exit(NULL);
    }

    if (*analyzer->script_path != '/')
    {
        snprintf(lua_analyzer, PATH_MAX, "%s/%s", ANALYZERS_DIR, analyzer->script_path);
    }
    else
    {
        strncpy(lua_analyzer, analyzer->script_path, PATH_MAX);
    }

    if (*analyzer->input_uri != '/')
    {
        snprintf(input_uri, PATH_MAX, "%s/%s", RUN_DIR, analyzer->input_uri);
    }
    else
    {
        strncpy(input_uri, analyzer->output_uri, PATH_MAX);
    }

    if (*analyzer->output_uri != '/')
    {
        snprintf(output_uri, PATH_MAX, "%s/%s", RUN_DIR, analyzer->output_uri);
    }
    else
    {
        strncpy(output_uri, analyzer->input_uri, PATH_MAX);
    }

    /*
     * Set thread name to the file name of the lua script
     */
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);

    //TODO: disable I/O library

    /* set local LUA paths */
    snprintf(lua_path, PATH_MAX - 1, "package.path = package.path .. \";lib/?.lua\"");
    luaL_loadstring(L, lua_path);
    lua_pcall(L, 0, LUA_MULTRET, 0);

    snprintf(lua_path, PATH_MAX - 1, "package.cpath = package.cpath .. \";lib/?.so\"");
    luaL_loadstring(L, lua_path);
    lua_pcall(L, 0, LUA_MULTRET, 0);

    syslog(LOG_INFO, "Loading %s", lua_analyzer);

    if (luaL_loadfile(L, lua_analyzer) || lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "luaL_loadfile %s failed - %s", lua_analyzer, lua_tostring(L, -1));
        pthread_exit(NULL);
    }

    /* globally set redis host and port number in the LUA engine */
    lua_pushstring(L, g_redis_host);
    lua_setglobal(L, "redis_host");
    lua_pushstring(L, g_redis_port);
    lua_setglobal(L, "redis_port");

    /* register functions */
    lua_pushcfunction(L, dragonfly_log);
    lua_setglobal(L, "dragonfly_log");

    lua_pushcfunction(L, dragonfly_iprep);
    lua_setglobal(L, "dragonfly_iprep");

    /* initialize the script */
    lua_getglobal(L, "initialize");
    if (lua_pcall(L, 0, 0, 0))
    {
        syslog(LOG_ERR, "%s error; %s", lua_analyzer, lua_tostring(L, -1));
        pthread_exit(NULL);
    }
    pthread_barrier_wait(&g_barrier);

    syslog(LOG_NOTICE, "Running %s\n", lua_analyzer);

    while (g_running)
    {
        if ((analyzer->input = dragonfly_io_open(input_uri, DF_IN)) < 0)
        {
            break;
        }
        if ((analyzer->output = dragonfly_io_open(output_uri, DF_OUT)) < 0)
        {
            break;
        }

        lua_analyzer_loop(L, analyzer->input, analyzer->output);

        dragonfly_io_close(analyzer->output);
        dragonfly_io_close(analyzer->input);
    }
    lua_close(L);

    syslog(LOG_NOTICE, "%s exiting", lua_analyzer);
    pthread_setspecific(g_threadContext, NULL);
    pthread_exit(NULL);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

static void launch_lua_threads(const char *dragonfly_root)
{
    struct stat sb;

    strncpy(g_root_dir, dragonfly_root, PATH_MAX);
    if (chdir(g_root_dir) != 0)
    {
        syslog(LOG_ERR, "unable to chdir() to : %s", g_root_dir);
        exit(EXIT_FAILURE);
    }

    if (g_chroot)
    {
        if (chroot(g_root_dir) != 0)
        {
            syslog(LOG_ERR, "unable to chroot() to : %s\n", g_root_dir);
            exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "chroot: %s\n", g_root_dir);
    }

    syslog(LOG_INFO, "chdir: %s\n", getcwd(g_root_dir, PATH_MAX));

/*
    snprintf(g_run_dir, PATH_MAX, "%s/%s", dragonfly_root, RUN_DIR);
    snprintf(g_analyzer_dir, PATH_MAX, "%s/%s", dragonfly_root, ANALYZERS_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_analyzer_dir, CONFIG_FILE);
*/

    strncpy(g_run_dir, RUN_DIR, PATH_MAX);
    strncpy(g_analyzer_dir, ANALYZERS_DIR, PATH_MAX);
    snprintf(g_log_dir, PATH_MAX, "%s", LOG_DIR);
    snprintf(g_config_file, PATH_MAX, "%s/%s", g_analyzer_dir, CONFIG_FILE);

    if ((lstat(g_config_file, &sb) < 0) || !S_ISREG(sb.st_mode))
    {
        fprintf(stderr, "%s does not exist.\n", g_config_file);
        syslog(LOG_WARNING, "%s does noet exist.\n", g_config_file);
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "root dir: %s\n", dragonfly_root);
    syslog(LOG_INFO, "run dir: %s\n", g_run_dir);
    syslog(LOG_INFO, "analyzer dir: %s\n", g_analyzer_dir);
    syslog(LOG_INFO, "config file: %s\n", g_config_file);

    memset(g_analyzer_list, 0, sizeof(g_analyzer_list));
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
        syslog(LOG_ERR, "lua_pcall failed; %s", lua_tostring(L, -1));
        abort();
    }

    lua_getglobal(L, "redis_port");
    if (lua_isstring(L, -1))
    {
        g_redis_host = strdup(lua_tostring(L, -1));
    }
    lua_getglobal(L, "redis_host");
    if (lua_isstring(L, -1))
    {
        g_redis_port = strdup(lua_tostring(L, -1));
    }

    static struct
    {
        const char *key;
        int type;
    } fields[] = {
        { .key="name", .type=LUA_TSTRING},
        { .key="input", .type=LUA_TSTRING},
        { .key="script", .type=LUA_TSTRING},
        { .key="output", .type=LUA_TSTRING}
    };

    lua_getglobal(L, "analyzers");
    luaL_checktype(L, -1, LUA_TTABLE);
    for (int i = 1;; i++)
    {
        if (i > MAX_ANALYZERS)
        {
            syslog(LOG_ERR, "Exceeded maximum number of supported analyzers: %d", MAX_ANALYZERS);
            abort();
        }
        
        lua_rawgeti(L, -1, i);
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
                g_analyzer_list[i].name = strdup(lua_tostring(L, -1));
                if (g_verbose)
                    printf("name: %s\n", g_analyzer_list[i].name);
                break;
            case 1:
                g_analyzer_list[i].input_uri = strndup(lua_tostring(L, -1), PATH_MAX);
                if (g_verbose)
                    printf("input: %s\n", g_analyzer_list[i].input_uri);
                break;
            case 2:
                g_analyzer_list[i].script_path = strndup(lua_tostring(L, -1), PATH_MAX);
                if (g_verbose)
                    printf("script: %s\n", g_analyzer_list[i].script_path);
            case 3:
                g_analyzer_list[i].output_uri = strndup(lua_tostring(L, -1), PATH_MAX);
                if (g_verbose)
                    printf("output: %s\n", g_analyzer_list[i].output_uri);
            }
            lua_pop(L, 1);
        }
        g_num_lua_threads++;
        lua_pop(L, 1);
    }
    lua_close(L);

    pthread_barrier_init(&g_barrier, NULL, g_num_lua_threads + 1);

    if (pthread_key_create(&g_threadContext, thread_context_destroy) != 0)
    {
        syslog(LOG_ERR, "pthread_key_create() %s", strerror(errno));
        pthread_exit(NULL);
    }

    for (int i = 0; g_analyzer_list[i].script_path != NULL; i++)
    {
        struct stat sb;
        char lua_analyzer[PATH_MAX];
        snprintf(lua_analyzer, PATH_MAX - 1, "%s/%s", ANALYZERS_DIR, g_analyzer_list[i].script_path);
        /*
         * check that file exists with execute permissions 
         */
        pthread_t thread;
        if ((lstat(lua_analyzer, &sb) >= 0) && S_ISREG(sb.st_mode))
        {
            if (pthread_create(&thread, NULL, lua_analyzer_thread, (void *)&g_analyzer_list[i]) != 0)
            {
                syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
                pthread_exit(NULL);
            }
        }
    }

    /*
     * Caution: The path below must be defined relative to chdir (g_run_dir) 
     *		so that it works if chroot() is in effect. See above.
     */
    dragonfly_iprep_init();

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

    signal(SIGHUP, dragonfly_log_rotate);
    /*
     * Wait until all analyzer threads are ready
     */
    pthread_barrier_wait(&g_barrier);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void launch_lua_analyzers(const char *root_directory)
{
    g_verbose = isatty(1);
    launch_lua_threads(root_directory);
}

    /*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

#ifdef UNIT_TEST
int main(void)
{
    openlog("dragonfly", LOG_PERROR, LOG_USER);
    dragonfly_start("dragonfly-config.lua");
    closelog();
}
#endif

/*
 * ---------------------------------------------------------------------------------------
 */

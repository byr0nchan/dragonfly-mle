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

#ifndef __CONFIG__
#define __CONFIG__

#include "dragonfly-io.h"
#include "msgqueue.h"

#define DRAGONFLY_ROOT "DRAGONFLY_ROOT"
#define RUN_DIR "run"
#define LOG_DIR "log"
#define FILTER_DIR "filter"
#define ANALYZER_DIR "analyzer"
#define CONFIG_DIR "config"
#define CONFIG_FILE "config/config.lua"

#define USER_NOBODY "nobody"

#define DRAGONFLY_LOG_TAG "{log}"
#define DRAGONFLY_STATS_TAG "{stats}"
#define DRAGONFLY_RUN_DIR "/var/run/dragonfly-mle"
#define DRAGONFLY_LOG_DIR "/var/log/dragonfly-mle"
#define DRAGONFLY_LOG_NAME "dragonfly-mle.log"
#define DRAGONFLY_DEFAULT_LOG "/var/log/dragonfly-mle/dragonfly-mle.log"
#define DRAGONFLY_LOG_STATS_NAME "dragonfly-mle-stats.log"
#define DRAGONFLY_ROOT_DIR "/usr/local/dragonfly-mle"
#define DRAGONFLY_STATS_LOG "/var/log/dragonfly-mle/dragonfly-mle-stats.log"
#define DRAGONFLY_LOG_INDEX 0
#define DRAGONFLY_STATS_INDEX 1

#define TMP_DIR "/tmp/"
typedef struct _MLE_TIMER_
{
    time_t epoch;
    size_t length;
    char *tag;
    char *msgpack;
    queue_t *queue;
} MLE_TIMER;

typedef struct _MLE_STATS_
{
    unsigned long input;
    unsigned long analysis;
    unsigned long output;
} MLE_STATS;

#define QUEUE_INPUT "/input_queue"
typedef struct _INPUT_CONFIG_
{
    char *tag;
    char *uri;
    char *script;
    char *default_analyzer;
    DF_HANDLE *input;
    queue_t *queue;
} INPUT_CONFIG;

#define QUEUE_OUTPUT "/output_queue"
typedef struct _OUTPUT_CONFIG_
{
    char *tag;
    char *uri;
    DF_HANDLE *output;
    queue_t *queue;
} OUTPUT_CONFIG;

#define QUEUE_ANALYZER "/analyzer_queue"
typedef struct _ANALYZER_CONFIG_
{
    char *tag;
    char *script;
    char *default_analyzer;
    char *default_output;
    queue_t *queue;
} ANALYZER_CONFIG;

#define QUEUE_RESPONSE "/response_queue"
typedef struct _RESPONDER_CONFIG_
{
    char *tag;
    char *name;
    char *param;
} RESPONDER_CONFIG;

#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/luajit.h>

int load_analyzers_config(lua_State *L, ANALYZER_CONFIG analyzer_list[], int max);
void unload_analyzers_config(ANALYZER_CONFIG analyzer_list[], int max);
int load_inputs_config(lua_State *L, INPUT_CONFIG input_list[], int max);
void unload_inputs_config(INPUT_CONFIG input_list[], int max);
int load_outputs_config(lua_State *L, OUTPUT_CONFIG output_list[], int max);
void unload_outputs_config(OUTPUT_CONFIG output_list[], int max);
int load_responder_config(lua_State *L, RESPONDER_CONFIG responder_list[], int max);
void unload_responder_config(RESPONDER_CONFIG responder_list[], int max);
int load_redis(lua_State *L, const char *host, int port);
#endif

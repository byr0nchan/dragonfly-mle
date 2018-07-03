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
#define SCRIPTS_DIR "scripts"
#define CONFIG_FILE "config.lua"

#define USER_NOBODY "nobody"
#define DRAGONFLY_ROOT_DIR "/opt/dragonfly"
#define DRAGONFLY_LOG_DIR "log"
#define DRAGONFLY_LOG_NAME "dragonfly.log"

#define QUEUE_INPUT "/input_queue"

typedef struct _MLE_STATS_
{
    unsigned long input;
    unsigned long analysis;
    unsigned long output;
} MLE_STATS;

typedef struct _INPUT_CONFIG_
{
    char *tag;
    char *uri;
    char *script;
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
    queue_t *queue;
} ANALYZER_CONFIG;

typedef struct _RESPONDER_CONFIG_
{
    char *tag;
    char *name;
    char *param;
} RESPONDER_CONFIG;

int load_analyzers_config(lua_State *L, ANALYZER_CONFIG analyzer_list[], int max);
void unload_analyzers_config(ANALYZER_CONFIG analyzer_list[], int max);
int load_inputs_config(lua_State *L, INPUT_CONFIG input_list[], int max);
void unload_inputs_config(INPUT_CONFIG input_list[], int max);
int load_outputs_config(lua_State *L, OUTPUT_CONFIG output_list[], int max);
void unload_outputs_config(OUTPUT_CONFIG output_list[], int max);
int load_responder_config(lua_State *L, RESPONDER_CONFIG responder_list[], int max);
void unload_responder_config(RESPONDER_CONFIG responder_list[], int max);
#endif

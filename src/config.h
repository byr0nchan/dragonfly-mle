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
#include "pipe.h"

typedef struct _INPUT_CONFIG_
{
    char *tag;
    char *uri;
    char *script;
    DF_HANDLE *input;
    pipe_t *pipe;
} INPUT_CONFIG;

typedef struct _OUTPUT_CONFIG_
{
    char *tag;
    char *uri;
    DF_HANDLE *output;
    pipe_t *pipe;
} OUTPUT_CONFIG;

typedef struct _ANALYZER_CONFIG_
{
    char *tag;
    char *script;
    pipe_t *pipe;
} ANALYZER_CONFIG;

typedef struct _RESPONDER_CONFIG_
{
    char *tag;
    char *name;
    char *parameter;
} RESPONDER_CONFIG;

int load_analyzers_config(lua_State *L, ANALYZER_CONFIG analyzer_list[], int max);
int load_inputs_config(lua_State *L, INPUT_CONFIG input_list[], int max);
int load_outputs_config(lua_State *L, OUTPUT_CONFIG output_list[], int max);
int load_responder_config(lua_State *L, RESPONDER_CONFIG responder_list[], int max);

#endif

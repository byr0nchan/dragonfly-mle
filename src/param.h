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

#ifndef __PARAM_H__
#define __PARAM_H__

#define MAX_WORKER_THREADS 1
#define MAX_ANALYZER_STREAMS (16)
#define MAX_INPUT_STREAMS (8)
#define MAX_OUTPUT_STREAMS (4+2)
#define MAX_RESPONDER_COMMANDS (2)
#define MAX_REDIS_MODULES   8
#define MAX_QUEUE_LENGTH 10
#define DEFAULT_STATS_INTERVAL 60

#define _MAX_BUFFER_SIZE_ (32768)

#endif

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


#ifdef RUN_UNIT_TESTS

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>

#include "analyzer-threads.h"
#include "dragonfly-io.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST0(const char *dragonfly_root)
{
	fprintf(stderr, "\n\n%s:(parsing config.lua)\n\n", __FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");
	initialize_configuration(dragonfly_root);
	sleep (1);
	void destroy_configuration();
	fprintf(stderr,"\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif
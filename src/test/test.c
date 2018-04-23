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

#include "test.h"

#include "analyzer-threads.h"
#include "dragonfly-io.h"
extern int g_running;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void run_self_tests (const char *dragonfly_root)
{
	fprintf(stderr,"RUNNING unit tests...\n");
	/*
	SELF_TEST0 (dragonfly_root);
	sleep (2);
	SELF_TEST1(dragonfly_root);
	sleep (2);
	SELF_TEST2(dragonfly_root);
	sleep (2);
	SELF_TEST3(dragonfly_root);
	sleep (2);
	SELF_TEST4(dragonfly_root);
	sleep (2);
	SELF_TEST5(dragonfly_root);
	sleep (2);
	SELF_TEST6(dragonfly_root);
	sleep (2);
	*/
	SELF_TEST7(dragonfly_root);
	exit(EXIT_SUCCESS);
}
/*
 * ---------------------------------------------------------------------------------------
 */
#endif

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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <string.h>

#ifdef __DEBUG__

#include "analyzer-threads.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST4(const char *dragonfly_root)
{
#ifdef _UTEST4_
	fprintf(stderr, "\n%s:(rotating log files)\n\n", __FUNCTION__);

#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST3(const char *dragonfly_root)
{
#ifdef _UTEST3_
	fprintf(stderr, "\n%s:(forwarding messages from sink to source)\n\n", __FUNCTION__);

#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST2(const char *dragonfly_root)
{
#ifdef _UTEST2_
	fprintf(stderr, "\n%s:(starting threads then shutting down threads)\n\n", __FUNCTION__);

#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST1(const char *dragonfly_root)
{
#ifdef _UTEST1_
	fprintf(stderr, "\n%s:(parsing config.lua)\n\n", __FUNCTION__);
	initialize_configuration(dragonfly_root);
#endif
}

#endif

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void run_self_tests(const char *dragonfly_root)
{
#ifdef __DEBUG__
	SELF_TEST1(dragonfly_root);
	SELF_TEST2(dragonfly_root);
	SELF_TEST3(dragonfly_root);
	SELF_TEST4(dragonfly_root);
	exit(EXIT_SUCCESS);
#endif
}
/*
 * ---------------------------------------------------------------------------------------
 */

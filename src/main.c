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

#include <sys/un.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <sys/stat.h>

#include <limits.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#ifdef __FreeBSD__
#include <pthread_np.h>
#include <sys/limits.h>
#endif

#include "dragonfly-lib.h"

int g_chroot = 0;
int g_verbose = 0;
int g_drop_priv = 0;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void print_usage()
{
	printf("Usage: dragonfly [-c -p -r <root dir> -l <log dir>] -v\n");
}


/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

int main(int argc, char **argv)
{
	int option = 0;
	char *dragonfly_log = NULL;
	char *dragonfly_root = NULL;

	while ((option = getopt(argc, argv, "cflpr:v")) != -1)
	{
		switch (option)
		{
			/* chroot */
		case 'c':
			g_chroot = 1;
			break;

			/* drop privilege */
		case 'p':
			g_drop_priv = 1;
			break;

			/* root directory */
		case 'r':
			dragonfly_root = strndup(optarg, PATH_MAX);
			break;

			/* log directory */
		case 'l':
			dragonfly_log = strndup(optarg, PATH_MAX);
			break;

			/* verbose */
		case 'v':
			g_verbose = 1;
			break;

		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif

	openlog("dragonfly", LOG_PERROR, LOG_USER);
	
#ifdef RUN_UNIT_TESTS
	dragonfly_mle_test(TMP_DIR);
#endif
	dragonfly_mle_run(dragonfly_root, dragonfly_log, DRAGONFLY_RUN_DIR);

	closelog();
	exit(EXIT_SUCCESS);
}

/*
 * ---------------------------------------------------------------------------------------
 */

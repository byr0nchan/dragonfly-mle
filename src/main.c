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

#include "config.h"
#include "test.h"
#include "worker-threads.h"

#define DRAGONFLY_ROOT "DRAGONFLY_ROOT"

int g_chroot = 0;
int g_verbose = 0;
int g_drop_priv = 0;

uint64_t g_msgSubscribed = 0;
uint64_t g_msgReceived = 0;
uint64_t g_running = 1;
char g_dragonfly_root[PATH_MAX] = {'\0'};
char *g_dragonfly_log = NULL;
char g_suricata_command_path[PATH_MAX];

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

void verify_runtime_environment()
{
	umask (022);
	if (!*g_dragonfly_root)
	{
		strncpy(g_dragonfly_root, DRAGONFLY_ROOT_DIR, PATH_MAX);
	}
	struct stat sb;
	if ((lstat(g_dragonfly_root, &sb) < 0) || !S_ISDIR(sb.st_mode))
	{
		fprintf(stderr, "DRAGONFLY_ROOT %s does not exist\n", g_dragonfly_root);
		exit(EXIT_FAILURE);
	}
	if (!g_dragonfly_log)
	{
		g_dragonfly_log = strndup(DRAGONFLY_LOG_DIR, PATH_MAX);
	}
	/*
	 * Make sure log directory exists
	 */
	if ((lstat(g_dragonfly_log, &sb) < 0) || !S_ISCHR(sb.st_mode))
	{
		if (mkdir(g_dragonfly_log, 0755) && errno != EEXIST)
		{
			fprintf(stderr, "mkdir (%s) error - %s\n", g_dragonfly_log, strerror(errno));
			syslog(LOG_WARNING, "mkdir (%s) error - %s\n", g_dragonfly_log, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	/*
	 * Make sure log directory exists
	 */
	if ((lstat(DRAGONFLY_RUN_DIR, &sb) < 0) || !S_ISCHR(sb.st_mode))
	{
		if (mkdir(DRAGONFLY_RUN_DIR, 0755) && errno != EEXIST)
		{
			fprintf(stderr, "mkdir (%s) error - %s\n", DRAGONFLY_RUN_DIR, strerror(errno));
			syslog(LOG_WARNING, "mkdir (%s) error - %s\n", DRAGONFLY_RUN_DIR, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

int main(int argc, char **argv)
{
	int option = 0;
	memset(g_dragonfly_root, 0, sizeof(g_dragonfly_root));
	memset(g_suricata_command_path, 0, sizeof(g_suricata_command_path));
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
			strncpy(g_dragonfly_root, optarg, PATH_MAX);
			break;

			/* log directory */
		case 'l':
			g_dragonfly_log = strndup(optarg, PATH_MAX);
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

	verify_runtime_environment();
	
#ifdef RUN_UNIT_TESTS
	run_self_tests(TMP_DIR);
#endif

	openlog("dragonfly", LOG_PERROR, LOG_USER);

#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	launch_lua_threads(g_dragonfly_root);

	closelog();
	exit(EXIT_SUCCESS);
}

/*
 * ---------------------------------------------------------------------------------------
 */

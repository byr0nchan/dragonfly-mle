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

#include "test.h"
#include "worker-threads.h"

#define DRAGONFLY_ROOT "DRAGONFLY_ROOT"
#define DRAGONFLY_DIR "/opt/dragonfly"
#define TMP_DIR "/tmp/"

int g_chroot = 0;
int g_verbose = 0;
int g_drop_priv = 0;
int g_flush_queue = 0;

uint64_t g_msgSubscribed = 0;
uint64_t g_msgReceived = 0;
uint64_t g_running = 1;
char g_dragonfly_root[PATH_MAX];
char g_suricata_command_path[PATH_MAX];

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

void print_usage()
{
	printf("Usage: dragonfly -c -p -v -r <root dir>\n");
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */

int main(int argc, char **argv)
{
	memset(g_dragonfly_root, 0, sizeof(g_dragonfly_root));
	memset(g_suricata_command_path, 0, sizeof(g_suricata_command_path));

	int option = 0;
	while ((option = getopt(argc, argv, "cfpr:v")) != -1)
	{
		switch (option)
		{
			/* chroot */
		case 'c':
			g_chroot = 1;
			break;

			/* flush message queues */
		case 'f':
			g_flush_queue = 1;
			break;

			/* drop privilege */
		case 'p':
			g_drop_priv = 1;
			break;

			/* root directory */
		case 'r':
			strncpy(g_dragonfly_root, optarg, PATH_MAX);
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

	if (!*g_dragonfly_root)
	{
		strncpy(g_dragonfly_root, DRAGONFLY_DIR, PATH_MAX);
	}
	struct stat sb;
	if ((lstat(g_dragonfly_root, &sb) < 0) || !S_ISDIR(sb.st_mode))
	{
		fprintf(stderr, "DRAGONFLY_ROOT %s does not exist\n", g_dragonfly_root);
		exit(EXIT_FAILURE);
	}

#ifdef RUN_UNIT_TESTS
	run_self_tests(TMP_DIR);
#endif

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	launch_lua_threads(g_dragonfly_root);

	while (g_running)
	{
		sleep(1);
	}

	closelog();
	exit(EXIT_SUCCESS);
}

/*
 * ---------------------------------------------------------------------------------------
 */

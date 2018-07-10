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
#include <errno.h>

#include "config.h"
#include "test.h"

#include "worker-threads.h"
#include "dragonfly-io.h"
extern int g_running;
extern int g_flush_queue;

#define WAIT_INTERVAL 2

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void run_self_tests(const char *dragonfly_root)
{
	fprintf(stderr, "Running unit tests\n");
    g_flush_queue=1;

	if (chdir(dragonfly_root) != 0)
	{
		syslog(LOG_ERR, "unable to chdir() to  %s", dragonfly_root);
		exit(EXIT_FAILURE);
	}
	char *path = getcwd(NULL, PATH_MAX);
	if (path == NULL)
	{
		syslog(LOG_ERR, "getcwd() error - %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "DRAGONFLY_ROOT: %s\n", path);
	free(path);

	char analyzer_dir[PATH_MAX];
	snprintf(analyzer_dir, sizeof(analyzer_dir), "%s/%s", dragonfly_root, ANALYZER_DIR);
	mkdir(analyzer_dir, 0755);

	char etl_dir[PATH_MAX];
	snprintf(etl_dir, sizeof(etl_dir), "%s/%s", dragonfly_root, ETL_DIR);
	mkdir(etl_dir, 0755);

	char config_dir[PATH_MAX];
	snprintf(config_dir, sizeof(config_dir), "%s/%s", dragonfly_root, CONFIG_DIR);
	mkdir(config_dir, 0755);

	//char log_dir[PATH_MAX];
	//snprintf(log_dir, sizeof(log_dir), "%s/log", dragonfly_root);
	//mkdir(log_dir, 0755);

	SELF_TEST0(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST1(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST2(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST3(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST4(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST5(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST6(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST7(dragonfly_root);
	sleep(WAIT_INTERVAL);

	SELF_TEST8(dragonfly_root);
	sleep(WAIT_INTERVAL);

	rmdir(analyzer_dir);
	rmdir(etl_dir);
	rmdir(config_dir);
	//rmdir(log_dir);
	exit(EXIT_SUCCESS);
}
/*
 * ---------------------------------------------------------------------------------------
 */
#endif

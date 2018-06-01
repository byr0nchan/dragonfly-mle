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
#include <assert.h>

#include "worker-threads.h"
#include "dragonfly-io.h"

#include "test.h"

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"ipc://input.ipc\", script=\"input.lua\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"test\", script=\"analyzer.lua\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
	"    { tag=\"log\", uri=\"file://test.log\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   analyze_event (\"test\", msg)\n"
	"end\n";

const char *ANALYZER_LUA =
	"function setup()\n"
	"  conn = assert(hiredis.connect())\n"
	"end\n"
	"function loop (msg)\n"
	"  assert(conn:command(\"PING\") == hiredis.status.PONG)\n"
	"end\n\n";

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void write_file(const char *file_path, const char *content)
{
	fprintf(stderr, "generated %s\n", file_path);
	FILE *fp = fopen(file_path, "w+");
	if (!fp)
	{
		perror(__FUNCTION__);
		return;
	}
	fputs(content, fp);
	fclose(fp);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST2(const char *dragonfly_root)
{
#define MAX_TEST2_MESSAGES 10000
	const char *analyzer_path = "./scripts/analyzer.lua";
	const char *input_path = "./scripts/input.lua";
	const char *config_path = "./scripts/config.lua";

	fprintf(stderr, "\n\n%s: connecting to redis, sending %u ping messages, then disconnecting\n", __FUNCTION__, MAX_TEST2_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */
	assert(chdir(dragonfly_root) == 0);
	char *path = get_current_dir_name();
	fprintf(stderr, "DRAGONFLY_ROOT: %s\n", path);
	free (path);
	write_file(config_path, CONFIG_LUA);
	write_file(input_path, INPUT_LUA);
	write_file(analyzer_path, ANALYZER_LUA);

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");
	startup_threads(dragonfly_root);

	sleep(1);
	DF_HANDLE *pump = dragonfly_io_open("ipc://input.ipc", DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		abort();
	}

	sleep(1);

	long mod = 0;
	for (long i = 0; i < MAX_TEST2_MESSAGES; i++)
	{
		char msg[128];
		for (int j = 0; j < (sizeof(msg) - 1); j++)
		{
			msg[j] = 'A' + (mod % 48);
			mod++;
		}
		if ( i && (i % 100000) == 0 )
		{
				fprintf(stderr, "\t%lu redis pings\n", i);
		}
		msg[sizeof(msg) - 1] = '\0';
		dragonfly_io_write(pump, msg);
	}
	sleep(2);
	shutdown_threads();

	dragonfly_io_close(pump);
	closelog();

	fprintf(stderr, "\nCleaning up files\n");
	remove(config_path);
	remove(input_path);
	remove(analyzer_path);
	fprintf(stderr, "-------------------------------------------------------\n\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif
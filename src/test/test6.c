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
#include <errno.h>
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

#define MAX_TEST6_MESSAGES 100000
#define QUANTUM (MAX_TEST6_MESSAGES/10)

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"tail://input.txt\", script=\"input.lua\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"test\", script=\"analyzer.lua\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
	"    { tag=\"log\", uri=\"ipc://output.ipc\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   analyze_event (\"test\", msg)\n"
	"end\n";

static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (msg)\n"
	"   output_event (\"log\", msg)\n"
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
static void *writer_thread(void *ptr)
{
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "writer");
#endif

	DF_HANDLE *pump = dragonfly_io_open("file://input.txt", DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
		perror(__FUNCTION__);
		abort();
	}
	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	for (unsigned long i = 0; i < MAX_TEST6_MESSAGES; i++)
	{
		char msg[128];
		for (int j = 0; j < (sizeof(msg) - 1); j++)
		{
			msg[j] = 'A' + (mod % 48);
			mod++;
		}
		msg[sizeof(msg) - 1] = '\0';
		msg[sizeof(msg) - 2] = '\n';
		if (dragonfly_io_write(pump, msg) < 0)
		{
			fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
			perror(__FUNCTION__);
			abort();
		}
	}
	dragonfly_io_close(pump);
	return (void *)NULL;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST6(const char *dragonfly_root)
{
	const char *analyzer_path = "./scripts/analyzer.lua";
	const char *input_path = "./scripts/input.lua";
	const char *config_path = "./scripts/config.lua";

	fprintf(stderr, "\n\n%s: tailing %d messages from input to output.ipc\n", __FUNCTION__, MAX_TEST6_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */
	assert(chdir(dragonfly_root) == 0);
	char *path = getcwd(NULL, PATH_MAX);
	if (path == NULL)
	{
		syslog(LOG_ERR, "getcwd() error - %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "DRAGONFLY_ROOT: %s\n", path);
	free(path);
	write_file(config_path, CONFIG_LUA);
	write_file(input_path, INPUT_LUA);
	write_file(analyzer_path, ANALYZER_LUA);

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	DF_HANDLE *input = dragonfly_io_open("ipc://output.ipc", DF_IN);
	if (!input)
	{
		perror(__FUNCTION__);
		abort();
	}

	startup_threads(dragonfly_root);
	sleep(1);

	pthread_t tinfo;
	if (pthread_create(&tinfo, NULL, writer_thread, (void *)NULL) != 0)
	{
		perror(__FUNCTION__);
		abort();
	}

	/*
	 * write messages walking the alphabet
	 */
	char buffer[4096];
	clock_t last_time = clock();
	for (unsigned long i = 0; i < MAX_TEST6_MESSAGES; i++)
	{
		int len = dragonfly_io_read(input, buffer, (sizeof(buffer) - 1));
		if (len < 0)
		{
			perror(__FUNCTION__);
			abort();
		}
		if ((i > 0) && (i % QUANTUM) == 0)
		{
			clock_t mark_time = clock();
			double elapsed_time = ((double)(mark_time - last_time)) / CLOCKS_PER_SEC; // in seconds
			double ops_per_sec = QUANTUM / elapsed_time;
			fprintf(stderr, "\t%6.2f/sec\n", ops_per_sec);
			last_time = mark_time;
		}
	}
	pthread_join(tinfo, NULL);
	shutdown_threads();
	dragonfly_io_close(input);
	closelog();

	fprintf(stderr, "Cleaning up files\n");
	remove(config_path);
	remove(input_path);
	remove(analyzer_path);
	fprintf(stderr, "-------------------------------------------------------\n\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif

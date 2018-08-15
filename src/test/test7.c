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

#define MAX_TEST7_MESSAGES 10000
#define QUANTUM (MAX_TEST7_MESSAGES / 10)
static int g_running = 1;

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"tail://input.txt\", script=\"filter.lua\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"test\", script=\"analyzer.lua\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
	"    { tag=\"log\", uri=\"ipc://test7.ipc\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   -- print (msg)\n"
	"   local tbl = cjson_safe.decode(msg)\n"
	"   dragonfly.analyze_event (\"test\", tbl)\n"
	"end\n";

static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (tbl)\n"
	"   dragonfly.output_event (\"log\", tbl.msg)\n"
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
static void *producer_thread(void *ptr)
{
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "writer");
#endif
	DF_HANDLE *pump = dragonfly_io_open("file://input.txt<", DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
		perror(__FUNCTION__);
		abort();
	}
	/*
	 * write messages walking the alphabet
	 */
	int truncate_record = (MAX_TEST7_MESSAGES/100);
	char buffer[1024];
	int mod = 0;
	int sleep_time = 50;
	for (unsigned long i = 0; i < MAX_TEST7_MESSAGES; i++)
	{
		if (i == truncate_record)
		{
			dragonfly_io_close(pump);
			unlink ("/var/log/dragonfly-mle/input.txt");
			fprintf(stderr, "SELF_TEST7: truncating file input.txt\n");
			pump = dragonfly_io_open("file://input.txt", DF_OUT);
			if (!pump)
			{
				fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
				perror(__FUNCTION__);
				abort();
			}
			sleep_time = 1;
		}
		char msg[128];
		for (int j = 0; j < (sizeof(msg) - 1); j++)
		{
			msg[j] = 'A' + (mod % 48);
			if (msg[j] == '\\')
				msg[j] = ' ';
			mod++;
		}
		msg[sizeof(msg) - 1] = '\0';
		snprintf(buffer, sizeof(buffer), "{ \"id\": %lu, \"msg\":\"%s\" }", i, msg);
		if (dragonfly_io_write(pump, buffer) < 0)
		{
			fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
			perror(__FUNCTION__);
			abort();
		}
		usleep(sleep_time);
	}
	g_running = 0;
	dragonfly_io_close(pump);
	sleep (1);
	return (void *)NULL;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST7(const char *dragonfly_root)
{
	fprintf(stderr, "\n\n%s: truncating file while tailing %d messages from input to output.ipc\n",
			__FUNCTION__, MAX_TEST7_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */

	write_file(CONFIG_TEST_FILE, CONFIG_LUA);
	write_file(FILTER_TEST_FILE, INPUT_LUA);
	write_file(ANALYZER_TEST_FILE, ANALYZER_LUA);
	
	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	DF_HANDLE *input = dragonfly_io_open("ipc://test7.ipc", DF_IN);
	if (!input)
	{
		perror(__FUNCTION__);
		abort();
	}
	pthread_t tinfo;
	if (pthread_create(&tinfo, NULL, producer_thread, (void *)NULL) != 0)
	{
		perror(__FUNCTION__);
		abort();
	}

	startup_threads(dragonfly_root);


	/*
	 * read messages
	 */
	unsigned long i = 0;
	char buffer[4096];
	clock_t last_time = clock();
	while (g_running)
	{
		int len = dragonfly_io_read(input, buffer, (sizeof(buffer) - 1));
		if (len < 0)
		{
			perror(__FUNCTION__);
			abort();
		}
		if ((++i > 0) && (i % QUANTUM) == 0)
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

	fprintf(stderr, "%s: cleaning up files\n", __FUNCTION__);
	remove(CONFIG_TEST_FILE);
	remove(FILTER_TEST_FILE);
	remove(ANALYZER_TEST_FILE);
	fprintf(stderr, "-------------------------------------------------------\n\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif

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
#include <errno.h>
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

extern int g_running;

#define MAX_TEST5_MESSAGES 10000
#define QUANTUM (MAX_TEST5_MESSAGES/10)

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"ipc://input.ipc\", script=\"filter.lua\"}\n"
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
	"   local tbl = cjson.decode(msg)\n"
	"   dragonfly.analyze_event (\"test\", tbl)\n"
	"end\n";

static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (tbl)\n"
	"  -- print (tbl.msg)\n"
	"   dragonfly.output_event (\"log\", tbl.msg)\n"
	"end\n\n";
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void write_file(const char *file_path, const char *content)
{
	fprintf(stderr, "%s: generated %s\n", __FUNCTION__, file_path);
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
static void *consumer_thread(void *ptr)
{
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "reader");
#endif
	DF_HANDLE *pump_in = dragonfly_io_open("ipc://output.ipc", DF_IN);
	if (!pump_in)
	{
		fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__);
		perror(__FUNCTION__);
		abort();
	}
	clock_t last_time = clock();
	/*
	 * write messages walking the alphabet
	 */
	for (long i = 0; i < MAX_TEST5_MESSAGES; i++)
	{
		char msg_in[1024];
		msg_in[sizeof(msg_in) - 1] = '\0';
		if (dragonfly_io_read(pump_in, msg_in, sizeof(msg_in)) <= 0)
		{
			break;
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
	dragonfly_io_close(pump_in);
	return (void *)NULL;
}
/*
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST5(const char *dragonfly_root)
{
	fprintf(stderr, "\n\n%s: pumping %d messages from output pipe to input pipe\n",
			__FUNCTION__, MAX_TEST5_MESSAGES);
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
	pthread_t tinfo;
	if (pthread_create(&tinfo, NULL, consumer_thread, (void *)NULL) != 0)
	{
		perror(__FUNCTION__);
		abort();
	}
	sleep (1);
	startup_threads(dragonfly_root);

	DF_HANDLE *pump_out = dragonfly_io_open("ipc://input.ipc", DF_OUT);
	if (!pump_out)
	{
		perror(__FUNCTION__);
		abort();
	}

	sleep (1);
	/*
	 * write messages walking the alphabet
	 */

	int mod = 0;
	char buffer [1024];
	for (long i = 0; i < MAX_TEST5_MESSAGES; i++)
	{
		char msg_out[128];
		for (int j = 0; j < (sizeof(msg_out) - 2); j++)
		{
			msg_out[j] = 'A' + (mod % 48);
			if (msg_out[j]=='\\') msg_out[j]=' ';
			mod++;
		}
		msg_out[sizeof(msg_out)-1] = '\0';

		int len = strnlen(msg_out, sizeof(msg_out));
		if (len <= 0)
		{
			fprintf(stderr, "%s:  length error!!!", __FUNCTION__);
			abort();
		}
		snprintf(buffer, sizeof(buffer), "{ \"id\": %lu, \"msg\":\"%s\" }", i, msg_out);
//fprintf (stderr,"%s: %i [%lu]\n", __FUNCTION__, __LINE__, i);
		dragonfly_io_write(pump_out, buffer);
	}
	pthread_join(tinfo, NULL);
	shutdown_threads();
	dragonfly_io_close(pump_out);

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

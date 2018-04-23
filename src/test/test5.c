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
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>

#include "analyzer-threads.h"
#include "dragonfly-io.h"
extern int g_running;

#define MAX_TEST5_MESSAGES 1000000

static void *reader_thread(void *ptr)
{
	fprintf (stderr,"%s: -----> line %d\n",__FUNCTION__, __LINE__);
    pthread_setname_np(pthread_self(), "reader");
	DF_HANDLE *pump_in = dragonfly_io_open("ipc:///tmp/test5out.ipc", DF_IN);
	if (!pump_in)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		exit (-1);
	}
	clock_t last_time = clock();
	/*
	 * write messages walking the alphabet
	 */
	for (long i = 0; i < MAX_TEST5_MESSAGES; i++)
	{
		char msg_in[128];
		msg_in[sizeof(msg_in) - 1] = '\0';
		dragonfly_io_read(pump_in, msg_in, sizeof(msg_in));
		if ((i>0)&&(i % 100000) == 0)
		{
			clock_t mark_time = clock();
			double elapsed_time = ((double)( mark_time - last_time))/CLOCKS_PER_SEC; // in seconds
			double ops_per_sec = 100000 / elapsed_time;
			fprintf(stderr, "%6.2f/sec\n", ops_per_sec);
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
	fprintf(stderr, "\n\n%s:(pumping %d messages from output pipe to input pipe)\n", __FUNCTION__, MAX_TEST5_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test5.lua";
	const char *config_path = "/tmp/analyzers/config.lua";

	mkdir(analyzer_dir, 0755);
	// generate a config.lua file
	const char *config = "analyzers = {   "
						 "{ name=\"test5\","
						 "input=\"ipc:///tmp/test5in.ipc\", "
						 "script=\"test5.lua\", "
						 "output=\"ipc:///tmp/test5out.ipc\"}"
						 "   }\n";
	FILE *fp = fopen(config_path, "w+");
	if (!fp)
	{
		perror(__FUNCTION__);
		return;
	}
	fprintf(stderr, "config.lua:\n\n%s\n\n", config);
	fputs(config, fp);
	fclose(fp);

	// generate a test.lua analyzer
	const char *script = "local count = 0 \n"
						 "function setup()\n"
						 " count = 0\n"
						 "end\n"
						 "function loop (msg)\n"
						 " count = count + 1\n"
						 " local now = os.time()\n"
						 " local dtg = os.date(\"!%Y-%m-%dT%TZ\",now)\n"
						 " event = \"event \"..tostring(count)\n"
						 " log_event (dtg, event, msg)\n"
						 "end\n\n";
	fp = fopen(script_path, "w+");
	if (!fp)
	{
		perror(__FUNCTION__);
		return;
	}
	fprintf(stderr, "test5.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose(fp);

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");

	pthread_attr_t attr;
	pthread_t tinfo;
	if (pthread_create(&tinfo, &attr, reader_thread, (void *)NULL) != 0)
	{
		syslog(LOG_ERR, "pthread_create() %s", strerror(errno));
		pthread_exit(NULL);
	}
	sleep (2);

	startup_threads(test_dir);

	DF_HANDLE *pump_out = dragonfly_io_open("ipc:///tmp/test5in.ipc", DF_OUT);
	if (!pump_out)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	for (long i = 0; i < MAX_TEST5_MESSAGES; i++)
	{
		char msg_out[128];
		for (int j = 0; j < (sizeof(msg_out) - 1); j++)
		{
			msg_out[j] = 'A' + (mod % 48);
			mod++;
		}
		dragonfly_io_write(pump_out, msg_out);
	}
	dragonfly_io_close(pump_out);
	shutdown_threads();

	closelog();
	remove(config_path);
	remove(script_path);

	rmdir(analyzer_dir);
	fprintf(stderr, "\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif
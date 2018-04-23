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

#include "analyzer-threads.h"
#include "dragonfly-io.h"

#define MAX_TEST6_MESSAGES 1000000

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST6(const char *dragonfly_root)
{

	fprintf(stderr, "\n\n%s:(pumping %d messages from tailed file to input pipe)\n", __FUNCTION__, MAX_TEST6_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test6.lua";
	const char *config_path = "/tmp/analyzers/config.lua";

	mkdir(analyzer_dir, 0755);
	// generate a config.lua file
	const char *config = "analyzers = {   "
						 "{ name=\"test6\","
						 "input=\"tail:///tmp/test6in.txt\", "
						 "script=\"test6.lua\", "
						 "output=\"ipc:///tmp/test6out.ipc\"}"
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
	fprintf(stderr, "test6.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose(fp);

	DF_HANDLE *pump_in = dragonfly_io_open("ipc:///tmp/test6out.ipc", DF_IN);
	if (!pump_in)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	// start the service
	// create a pipe for sending messages
	// pump messages through the pipe
	// shutdown the service
	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");
	startup_threads(test_dir);
	sleep(2);
	DF_HANDLE *pump_out = dragonfly_io_open("file:///tmp/test6in.txt", DF_OUT);
	if (!pump_out)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	/*
         * write messages walking the alphabet
         */
	clock_t last_time = clock();
	int mod = 0;
	for (long i = 0; i < MAX_TEST6_MESSAGES; i++)
	{
		char msg_out[64];
		char msg_in[64];
		for (int j = 0; j < sizeof(msg_out); j++)
		{
			msg_out[j] = 'A' + (mod % 48);
			mod++;
		}
		msg_out[sizeof(msg_out) - 2] = '\n';
		msg_out[sizeof(msg_out) - 1] = '\0';
		//fprintf (stderr, "%s", msg_out);
		dragonfly_io_write(pump_out, msg_out);
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
	dragonfly_io_close(pump_out);
	shutdown_threads();

	closelog();
	remove(config_path);
	remove(script_path);
	remove("/tmp/test6.log");
	rmdir(analyzer_dir);
	fprintf(stderr, "\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif

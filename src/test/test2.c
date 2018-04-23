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

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST2(const char *dragonfly_root)
{

#define MAX_TEST2_MESSAGES 1000000
	fprintf(stderr, "\n\n%s:connecting to redis, sending ping, then disconnecting\n", __FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test2.lua";
	const char *config_path = "/tmp/analyzers/config.lua";
	mkdir (analyzer_dir, 0755);
	// generate a config.lua file 
	const char* config = "analyzers = {   "
									 "{ name=\"test2\","
									   "input=\"ipc:///tmp/test2.ipc\", "
									   "script=\"test2.lua\", "
									   "output=\"file:///tmp/test2.log\"}"
								     "   }\n";
	FILE *fp = fopen(config_path,"w+");
	if (!fp)
	{
		perror (__FUNCTION__);
		return;
	}
	fprintf (stderr, "config.lua:\n\n%s\n\n", config);
	fputs(config, fp);
	fclose (fp);
	
	// generate a test.lua analyzer
	const char* script = "local conn \n"
						 "local count = 0 \n"
					     "function setup()\n"
						 "  conn = assert(hiredis.connect())\n"
						 "  print(\"setup()\")\n"
						 "end\n"
					     "function loop (msg)\n"
						 "  assert(conn:command(\"PING\") == hiredis.status.PONG)\n"
						 "  count = count + 1\n"
						 "  local now = os.time()\n"
      					 "  local dtg = os.date(\"!%Y-%m-%dT%TZ\",now)\n"
						 "  event = \"event \"..tostring(count)\n"
						 "  assert(conn:command(\"SET\", count, dtg,\"5\"))\n"
						 "  log_event (dtg, event, msg)\n"
						 "end\n\n";
	fp = fopen(script_path,"w+");
	if (!fp)
	{
		perror (__FUNCTION__);
		return;
	}
	fprintf (stderr, "test2.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose (fp);


	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");
    startup_threads(test_dir);

	sleep (1);
	DF_HANDLE *pump = dragonfly_io_open("ipc:///tmp/test2.ipc", DF_OUT);
	if (!pump)
	{
		fprintf (stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	sleep(1);
	int mod = 0;
	for (int i=0; i< MAX_TEST2_MESSAGES; i++)
	{
		char msg [128];
		for (int j=0; j<(sizeof(msg)-1);j++)
		{
			msg [j]='A'+(mod%48);
			mod++;
		}
		msg [sizeof(msg)-1]='\0';
    	dragonfly_io_write (pump, msg);
	}
    sleep (1);
	shutdown_threads();
	dragonfly_io_close (pump);

	closelog();
    remove (config_path);
    remove (script_path);
	remove ("/tmp/test2.log");
	rmdir (analyzer_dir);
	fprintf(stderr,"\n");

}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif
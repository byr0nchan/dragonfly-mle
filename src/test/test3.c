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
void SELF_TEST3(const char *dragonfly_root)
{

#define MAX_TEST3_MESSAGES 100000
	fprintf(stderr, "\n\n%s:(pumping %d messages)\n", __FUNCTION__, MAX_TEST3_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test3.lua";
	const char *config_path = "/tmp/analyzers/config.lua";
	mkdir (analyzer_dir, 0755);
	// generate a config.lua file 
	const char* config = "analyzers = {   "
									 "{ name=\"test3\","
									   "input=\"ipc:///tmp/test3.ipc\", "
									   "script=\"test3.lua\", "
									   "output=\"file:///tmp/test3.log\"}"
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
	const char* script = "local count = 0 \n"
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
	fp = fopen(script_path,"w+");
	if (!fp)
	{
		perror (__FUNCTION__);
		return;
	}
	fprintf (stderr, "test3.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose (fp);
	
	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");
    startup_threads(test_dir);
	sleep (1);
	
	DF_HANDLE *pump = dragonfly_io_open("ipc:///tmp/test3.ipc", DF_OUT);
	if (!pump)
	{
		fprintf (stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	for (int i=0; i< MAX_TEST3_MESSAGES; i++)
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
	dragonfly_io_close (pump);
	shutdown_threads();

	closelog();
    remove (config_path);
    remove (script_path);
	remove ("/tmp/test3.log");
	rmdir (analyzer_dir);
	fprintf(stderr,"\n");


}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif
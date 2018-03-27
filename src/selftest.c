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

#ifdef __DEBUG__

#include "analyzer-threads.h"
#include "dragonfly-io.h"
extern int g_running;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST5(const char *dragonfly_root)
{
#ifdef _UTEST5_
#define MAX_TEST5_MESSAGES 1000000
	fprintf(stderr, "\n\n%s:(pumping %d messages from output pipe to input pipe)\n", __FUNCTION__, MAX_TEST5_MESSAGES);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test5.lua";
	const char *config_path = "/tmp/analyzers/config.lua";

	mkdir (analyzer_dir, 0755);
	// generate a config.lua file 
	const char* config = "analyzers = {   "
									 "{ name=\"test5\","
									   "input=\"ipc:///tmp/test5in.ipc\", "
									   "script=\"test5.lua\", "
									   "output=\"ipc:///tmp/test5out.ipc\"}"
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
						 " dragonfly_log (dtg, event, msg)\n"
						 "end\n\n";
	fp = fopen(script_path,"w+");
	if (!fp)
	{
		perror (__FUNCTION__);
		return;
	}
	fprintf (stderr, "test5.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose (fp);
	
	DF_HANDLE *pump_in = dragonfly_io_open("ipc:///tmp/test5out.ipc", DF_IN);
	if (!pump_in)
	{
		fprintf (stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
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
	sleep (1);
	
	DF_HANDLE *pump_out = dragonfly_io_open("ipc:///tmp/test5in.ipc", DF_OUT);
	if (!pump_out)
	{
		fprintf (stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	for (long i=0; i< MAX_TEST5_MESSAGES; i++)
	{
		char msg_out [128];
		char msg_in [128];
		for (int j=0; j<(sizeof(msg_out)-1);j++)
		{
			msg_out [j]='A'+(mod%48);
			mod++;
		}
		msg_in [sizeof(msg_out)-1]='\0';
    	dragonfly_io_write (pump_out, msg_out);
		dragonfly_io_read (pump_in, msg_in, sizeof(msg_in));
		if ((i % 100000)==0)
		{
			fprintf (stderr, "%lu\n", i);
		}
	}
	dragonfly_io_close (pump_in);
	dragonfly_io_close (pump_out);
	shutdown_threads();

	closelog();
    remove (config_path);
    remove (script_path);
	remove ("/tmp/test4.log");
	rmdir (analyzer_dir);
	fprintf(stderr,"\n");
#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST4(const char *dragonfly_root)
{
#ifdef _UTEST4_
#define MAX_TEST4_MESSAGES 150000
	int halfway = (MAX_TEST4_MESSAGES/2);
	fprintf(stderr, "\n\n%s:(pumping %d messages and rotating at %d)\n", __FUNCTION__, MAX_TEST4_MESSAGES, halfway);
	fprintf(stderr, "-------------------------------------------------------\n");

	const char *test_dir = "/tmp";
	const char *analyzer_dir = "/tmp/analyzers";
	const char *script_path = "/tmp/analyzers/test4.lua";
	const char *config_path = "/tmp/analyzers/config.lua";

	mkdir (analyzer_dir, 0755);
	// generate a config.lua file 
	const char* config = "analyzers = {   "
									 "{ name=\"test4\","
									   "input=\"ipc:///tmp/test4.ipc\", "
									   "script=\"test4.lua\", "
									   "output=\"file:///tmp/test4.log\"}"
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
						 " dragonfly_log (dtg, event, msg)\n"
						 "end\n\n";
	fp = fopen(script_path,"w+");
	if (!fp)
	{
		perror (__FUNCTION__);
		return;
	}
	fprintf (stderr, "test4.lua:\n\n%s\n\n", script);
	fputs(script, fp);
	fclose (fp);
	

	// start the service
	// create a pipe for sending messages
	// pump messages through the pipe
	// shutdown the service
	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
	pthread_setname_np(pthread_self(), "dragonfly");
    startup_threads(test_dir);
	sleep (1);
	
	DF_HANDLE *pump = dragonfly_io_open("ipc:///tmp/test4.ipc", DF_OUT);
	if (!pump)
	{
		fprintf (stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		return;
	}

	/*
	 * write messages walking the alphabet
	 */
	int mod = 0;
	for (int i=0; i< MAX_TEST4_MESSAGES; i++)
	{
		char msg [128];
		for (int j=0; j<(sizeof(msg)-1);j++)
		{
			msg [j]='A'+(mod%48);
			mod++;
		}
		msg [sizeof(msg)-1]='\0';
    	dragonfly_io_write (pump, msg);
		if (i==halfway)
		{
			kill(getpid(), SIGUSR1);
		}
	}
	dragonfly_io_close (pump);
	shutdown_threads();

	closelog();
    remove (config_path);
    remove (script_path);
	remove ("/tmp/test4.log");
	rmdir (analyzer_dir);
	fprintf(stderr,"\n");
#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST3(const char *dragonfly_root)
{
#ifdef _UTEST3_
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
						 " dragonfly_log (dtg, event, msg)\n"
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

#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST2(const char *dragonfly_root)
{
#ifdef _UTEST2_
#define MAX_TEST2_MESSAGES 100
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
						 "	conn = assert(hiredis.connect())\n"
						 "  assert(conn:command(\"PING\") == hiredis.status.PONG)\n"
						 "  count = count + 1\n"
						 "  local now = os.time()\n"
      					 "  local dtg = os.date(\"!%Y-%m-%dT%TZ\",now)\n"
						 "  event = \"event \"..tostring(count)\n"
						 "  assert(conn:command(\"SET\", count, dtg,\"5\"))\n"
						 "  dragonfly_log (dtg, event, msg)\n"
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

#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST1(const char *dragonfly_root)
{
#ifdef _UTEST1_
	fprintf(stderr, "\n\n%s:(starting threads then shutting down threads)\n\n", __FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");
	startup_threads (dragonfly_root);
	sleep (3);
	shutdown_threads();
	fprintf(stderr,"\n");
#endif
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void SELF_TEST0(const char *dragonfly_root)
{
#ifdef _UTEST0_
	fprintf(stderr, "\n\n%s:(parsing config.lua)\n\n", __FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");
	initialize_configuration(dragonfly_root);
	sleep (1);
	void destroy_configuration();
	fprintf(stderr,"\n");
#endif
}

#endif

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void run_self_tests(const char *dragonfly_root)
{
#ifdef __DEBUG__
	SELF_TEST0 (dragonfly_root);
	SELF_TEST1(dragonfly_root);
	SELF_TEST2(dragonfly_root);
	SELF_TEST3(dragonfly_root);
	SELF_TEST4(dragonfly_root);
	SELF_TEST5(dragonfly_root);
	exit(EXIT_SUCCESS);
#endif
}
/*
 * ---------------------------------------------------------------------------------------
 */

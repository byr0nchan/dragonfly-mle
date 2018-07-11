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

static const char *CONFIG_LUA =
	"inputs = {\n"
	"   { tag=\"input\", uri=\"ipc://input.ipc\", script=\"etl.lua\"}\n"
	"}\n"
	"\n"
	"analyzers = {\n"
	"    { tag=\"test\", script=\"analyzer.lua\" },\n"
	"}\n"
	"\n"
	"outputs = {\n"
	"    { tag=\"log\", uri=\"file://test8.log\"},\n"
	"}\n"
	"\n";

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   local tbl = cjson.decode(msg)\n"
	"   analyze_event (\"test\", tbl)\n"
	"end\n";

static const char *ANALYZER_LUA =
	"filename = \"sslblacklist.csv\"\n"
	"function split(s, delimiter)\n"
	"		result = {}\n"
	"		for match in (s..delimiter):gmatch(\"(.-)\"..delimiter) do\n"
	"   		 table.insert(result, match)\n"
	"		end\n"
	"		return result\n"
	"end\n"
	"function setup()\n"
	"   http_get (\"https://sslbl.abuse.ch/blacklist/sslblacklist.csv\",filename)\n"
	"   conn = assert(hiredis.connect())\n"
	"   assert(conn:command(\"PING\") == hiredis.status.PONG)\n"
	"	local file, err = io.open(filename, \'rb\')\n"
	"	if file then\n"
	"		while true do\n"
	"			line = file:read()\n"
	"	   		if line and not line:find(\"^#\") then\n"
	"				-- print(line)\n"
	"				tokens = split (line,\',\')\n"
	"				if tokens[1] then\n"
	"					assert(conn:command(\"HMSET\",tokens[2],\"dtg\", tokens[1], \"cat\", tokens[3]) == hiredis.status.OK)\n"
	"					print (tokens[1], tokens[2], tokens[3])\n"
	"				end\n"
	"			end\n"
	"		end\n"
	"	else\n"
	"		error(filename .. \": \" .. err)\n"
	"	end\n"
	"end\n"
	"function loop (tbl)\n"
	"   output_event (\"log\", tbl.msg)\n"
	"end\n";

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
void SELF_TEST8(const char *dragonfly_root)
{
	const char *analyzer_path = "./analyzer/analyzer.lua";
	const char *input_path = "./etl/etl.lua";
	const char *config_path = "./config/config.lua";

	fprintf(stderr, "\n\n%s: http_get followed by sending a message\n",
			__FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");
	/*
	 * generate lua scripts
	 */
	write_file(config_path, CONFIG_LUA);
	write_file(input_path, INPUT_LUA);
	write_file(analyzer_path, ANALYZER_LUA);

	signal(SIGPIPE, SIG_IGN);
	openlog("dragonfly", LOG_PERROR, LOG_USER);
#ifdef _GNU_SOURCE
	pthread_setname_np(pthread_self(), "dragonfly");
#endif
	startup_threads(dragonfly_root);

	sleep(1);
	DF_HANDLE *pump = dragonfly_io_open("ipc://input.ipc", DF_OUT);
	if (!pump)
	{
		fprintf(stderr, "%s: dragonfly_io_open() failed.\n", __FUNCTION__);
		abort();
	}

	sleep(1);
	int mod = 0;
	char msg[64];
	char buffer[1024];
	for (unsigned int j = 0; j < (sizeof(msg) - 1); j++)
	{
		msg[j] = 'A' + (mod % 48);
		if (msg[j] == '\\')
			msg[j] = ' ';
		mod++;
	}
	msg[sizeof(msg) - 1] = '\0';
	snprintf(buffer, sizeof(buffer), "{ \"id\": %lu, \"msg\":\"%s\" }", (unsigned long) 1, msg);
	dragonfly_io_write(pump, buffer);

	sleep(2);
	shutdown_threads();
	sleep(2);

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

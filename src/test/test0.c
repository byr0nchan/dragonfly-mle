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
#include <errno.h>

#include "worker-threads.h"
#include "dragonfly-io.h"
#include "test.h"

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
	"    { tag=\"log\", uri=\"file://test0.log\"},\n"
	"}\n"
	"\n";

#ifdef COMMENT_OUT
"responders = {\n"
	"    { tag=\"respond\", param=\"file://respond.ipc\"},\n"
	"}\n"
	"\n";
#endif

static const char *INPUT_LUA =
	"function setup()\n"
	"end\n"
	"\n"
	"function loop(msg)\n"
	"   local tbl = cjson_safe.decode(msg)\n"
	"   dragonfly.analyze_event (\"test\", tbl)\n"
	"end\n";

// generate a test.lua analyzer
static const char *ANALYZER_LUA =
	"function setup()\n"
	"end\n"
	"function loop (msg)\n"
	"  dragonfly.output_event (\"log\", msg)\n"
	"end\n\n";


/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void write_file(const char *file_path, const char *content)
{
	fprintf(stderr, "Generated %s\n", file_path);
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
 * ------------------------------_--------------------------------------------------------
 */
void SELF_TEST0(const char *dragonfly_root)
{
	fprintf(stderr, "\n\n%s: parsing config.lua\n", __FUNCTION__);
	fprintf(stderr, "-------------------------------------------------------\n");

	/*
	 * generate lua scripts
	 */
	char *path = getcwd(NULL, PATH_MAX);
	if (path == NULL)
	{
		syslog(LOG_ERR, "getcwd() error - %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	}
	fprintf(stderr, "%s: DRAGONFLY_ROOT: %s\n", __FUNCTION__, path);
	free(path);
	write_file(CONFIG_TEST_FILE, CONFIG_LUA);
	write_file(FILTER_TEST_FILE, INPUT_LUA);
	write_file(ANALYZER_TEST_FILE, ANALYZER_LUA);
	/*
	 * load configuration
	 */
	fprintf(stderr, "%s: Loading configuration file...", __FUNCTION__);
	initialize_configuration(dragonfly_root);
	fprintf(stderr, ".done.\n");

	sleep(1);

	fprintf(stderr, "%s: Unloading configuration file...", __FUNCTION__);
	destroy_configuration();
	fprintf(stderr, ".done.\n");

	fprintf(stderr, "-------------------------------------------------------\n\n");
}

/*
 * ---------------------------------------------------------------------------------------
 */
#endif

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

#include <pwd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#include <suricata-cmds.h>
#include <dragonfly-cmds.h>

extern int g_verbose;
static int g_iprep = 0;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void dragonfly_iprep_init(const char *command_socket_path)
{
        /*
         * Establish connnection with the Suricata command channel, used for hostbits, if it exists
         * 
         * Caution: suri_command_connect() operates relative to chdir (g_run_dir) 
         *	    so that it works if chroot() is in effect. See above.
         */
        if (suricata_command_connect(command_socket_path) < 0)
        {
#ifdef __DEBUG__
                fprintf(stderr, "%s: failed to connect\n", __FUNCTION__);
#endif
                syslog(LOG_ERR, "suricata_command_connect failed");
        }
        g_iprep = 1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_date2epoch(lua_State *L)
{

        if (lua_gettop(L) != 1)
        {
                return luaL_error(L, "expecting exactly 1 argument");
        }
        struct tm epoch;
        const char *timestamp = luaL_checkstring(L, 1);
        strptime(timestamp, "%FT%T", &epoch);
        lua_pushnumber(L, mktime(&epoch));
        return 1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_iprep(lua_State *L)
{
        // Is there a connection to the command socket?
        if (!g_iprep)
        {
#ifdef __DEBUG__
                fprintf(stderr, "%s: failed to connect\n", __FUNCTION__);
#endif
                return -1;
        }

        if (lua_gettop(L) != 4)
        {
                return luaL_error(L, "expecting exactly 4 arguments");
        }

        const char *timestamp = luaL_checkstring(L, 1);
        const char *hostbit = luaL_checkstring(L, 2);
        const char *iplist = luaL_checkstring(L, 3);
        const int expire = luaL_checkint(L, 4);

#ifdef __DEBUG__
        fprintf(stderr, "%s: %s %s %s %i\n", __FUNCTION__, timestamp, hostbit, iplist, expire);
#endif
        int status = suricata_add_hostbit(timestamp, hostbit, iplist, expire);
        lua_pushnumber(L, status);
        return 1;
}

/*
 * ---------------------------------------------------------------------------------------
 */

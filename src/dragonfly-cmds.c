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

#include <curl/curl.h>

#include <dragonfly-cmds.h>

extern int g_verbose;
extern pthread_key_t g_threadContext;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static size_t http_write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
        size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
        return written;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int http_get(const char *url, const char *filename)
{
        CURL *curl;
        CURLcode ret;

        curl_global_init(CURL_GLOBAL_DEFAULT);

        curl = curl_easy_init();
        if (curl)
        {
                curl_easy_setopt(curl, CURLOPT_URL, url);

                /*
     * If you want to connect to a site who isn't using a certificate that is
     * signed by one of the certs in the CA bundle you have, you can skip the
     * verification of the server's certificate. This makes the connection
     * A LOT LESS SECURE.
     *
     * If you have a CA cert for the server stored someplace else than in the
     * default bundle, then the CURLOPT_CAPATH option might come handy for
     * you.
     */
#ifdef __DEBUG__
                fprintf(stderr, "%s: verifying peer certificate\n",__FUNCTION__);
#endif
                      curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

                /*
     * If the site you're connecting to uses a different host name that what
     * they have mentioned in their server certificate's commonName (or
     * subjectAltName) fields, libcurl will refuse to connect. You can skip
     * this check, but this will make the connection less secure.
     */
#ifdef __DEBUG__
                fprintf(stderr, "%s: verifying host name\n",__FUNCTION__);
#endif
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

                /* send all data to this function  */
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_data);

                /* Switch on full protocol/debug output while testing */
                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

                FILE *pagefile = fopen(filename, "wb");
                if (pagefile)
                {

                        /* write the page body to this file handle */
                        curl_easy_setopt(curl, CURLOPT_WRITEDATA, pagefile);

                        /* get it! */
                        ret = curl_easy_perform(curl);
                        /* Check for errors */
                        if (ret != CURLE_OK)
                                //TODO: return error string to LUA
                                fprintf(stderr, "curl_easy_perform() failed: %s\n",
                                        curl_easy_strerror(ret));

                        /* close the header file */
                        fclose(pagefile);
                }

                /* always cleanup */
                curl_easy_cleanup(curl);
        }

        curl_global_cleanup();

        return 0;
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_http_get(lua_State *L)
{
        if (lua_gettop(L) != 2)
        {
                return luaL_error(L, "expecting exactly 2 arguments");
        }
        const char *url = luaL_checkstring(L, 1);
        const char *filename = luaL_checkstring(L, 1);
        if (http_get(url, filename) < 0)
        {
                // do error

        }
        return 0;
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

#ifdef COMMENT_OUT
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_iprep(lua_State *L)
{
        // Is there a connection to the command socket?
        if (!g_suricata_command_path)
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
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_output(lua_State *L)
{
        if (lua_gettop(L) != 4)
        {
                return luaL_error(L, "expecting exactly 4 arguments");
        }
        const char *name = luaL_checkstring(L, 1);
        const char *timestamp = luaL_checkstring(L, 2);
        const char *event = luaL_checkstring(L, 3);
        const char *message = luaL_checkstring(L, 4);

        //OUTPUT_CONFIG *analyzer = (OUTPUT_CONFIG *)pthread_getspecific(g_threadContext);

        char buffer[4096];
        snprintf(buffer, sizeof(buffer) - 1,
                 "{\"timestamp\":\"%s\",\"event_type\":\"%s\",\"notice\":{\"category\":\"%s\",\"message\":\"%s\"}}\n",
                 timestamp, name, event, message);
        dragonfly_io_write(analyzer->output, buffer);
        return 0;
}
#endif
/*
 * ---------------------------------------------------------------------------------------
 */

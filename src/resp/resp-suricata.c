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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include <resp-suricata.h>

#define VERSION_MSG "{ \"version\": \"0.1\" }"

static int g_command_socket = -1;
static char *g_command_path = NULL;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int suricata_reconnect()
{
    return suricata_initialize(g_command_path);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int suricata_initialize(const char *command_path)
{
    if (!command_path)
    {
        return -1;
    }
    /* connect to the Suricata command socket */
    if ((g_command_socket < 0) && (g_command_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
#ifdef __DEBUG__
        fprintf(stderr, "%s: unable to create socket: %s (%i)\n", __FUNCTION__, strerror(errno), errno);
#endif
        return -1;
    }

    int s;
    char buffer[128];
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, command_path, sizeof(addr.sun_path));
    memset(buffer, 0, sizeof(buffer));

    if ((s = connect(g_command_socket, (struct sockaddr *)&addr, sizeof(addr))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to connect to %s: %s (%i)\n", __FUNCTION__, addr.sun_path, strerror(errno), errno);
#ifdef __DEBUG__
        fprintf(stderr, "%s: unable to connect %s: %s (%i)\n", __FUNCTION__, addr.sun_path, strerror(errno), errno);
#endif
        return -1;
    }
    syslog(LOG_INFO, "%s: %s", __FUNCTION__, command_path);

    if ((s = send(g_command_socket, VERSION_MSG, strlen(VERSION_MSG), 0)) < 0)
    {
        syslog(LOG_ERR, "%s: unable to send socket message: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    if ((s = read(g_command_socket, buffer, sizeof(buffer))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to read socket message: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    if (!g_command_path)
    {
        g_command_path = strdup(command_path);
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int suricata_command(const char *command, char* response, int max_response)
{
    int s = -1;
    int len = strnlen(command, 4096);
    while ((s = send(g_command_socket, command, len, 0)) < 0)
    {
        syslog(LOG_ERR, "%s: unable to send socket message: %s\n", __FUNCTION__, strerror(errno));
        if (s == ECONNRESET)
        {
            suricata_reconnect();
        }
        else
        {
            return -1;
        }
    }
    memset(response, 0, max_response);
    if ((s = read(g_command_socket, response, (max_response - 1))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to read socket message: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 */

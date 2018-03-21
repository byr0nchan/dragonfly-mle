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
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/un.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>

#include <suricata-cmds.h>

#define VERSION_MSG "{ \"version\": \"0.1\" }"

static int g_command_socket = -1;

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int suricata_command_connect()
{
    /* connect to the Suricata command socket */
    if ((g_command_socket < 0) && (g_command_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        syslog(LOG_ERR, "unable to create socket: %s (%i)\n", strerror(errno), errno);
        return -1;
    }

    int s;
    char buffer[128];
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    char command_path[PATH_MAX];

    snprintf(command_path, PATH_MAX - 1, "%s/suricata-command.ipc", RUN_DIR);

    strncpy(addr.sun_path, command_path, sizeof(addr.sun_path));
    memset(buffer, 0, sizeof(buffer));

    if ((s = connect(g_command_socket, (struct sockaddr *)&addr, sizeof(addr))) < 0)
    {
        syslog(LOG_ERR, "unable to connect to socket: %s - %s\n", command_path, strerror(errno));
        return -1;
    }
    syslog(LOG_INFO, "%s: %s", __FUNCTION__, command_path);
    if ((s = send(g_command_socket, VERSION_MSG, strlen(VERSION_MSG), 0)) < 0)
    {
        syslog(LOG_ERR, "unable to send socket message: %s\n", strerror(errno));
        return -1;
    }
    if ((s = read(g_command_socket, buffer, sizeof(buffer))) < 0)
    {
        syslog(LOG_ERR, "unable to read socket message: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int suricata_add_hostbit(const char *time, const char *hostbit_name, const char *iplist, int expire)
{
    int s = -1;
    char *address;
    char command[256];
    char *list = strndup(iplist, sizeof(command) - 1);

    while ((address = strtok_r(list, " ", &list)))
    {
        struct in_addr test;
        if (inet_aton(address, &test) == 0)
        {
            syslog(LOG_ERR, "%s: invalid address: %s\n", __FUNCTION__, address);
            continue;
        }

        int len = snprintf(command, sizeof(command) - 1,
                           "{\"command\":\"add-hostbit\", \"arguments\": { \"ipaddress\":\"%s\", \"hostbit\":\"%s\", \"expire\":%i } }",
                           address, hostbit_name, expire);

        while ((s = send(g_command_socket, command, len, 0)) < 0)
        {
            syslog(LOG_ERR, "unable to send socket message: %s\n", strerror(errno));
            if (s == ECONNRESET)
            {
                suricata_command_connect();
            }
            else
                return -1;
        }
        memset(command, 0, sizeof(command));
        if ((s = read(g_command_socket, command, sizeof(command))) < 0)
        {
            syslog(LOG_ERR, "unable to read socket message: %s\n", strerror(errno));
        }
    }
    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 */

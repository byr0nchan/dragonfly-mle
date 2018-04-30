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
#include <pthread.h>

#include <dragonfly-io.h>
#include <io-suricata.h>

#define VERSION_MSG "{ \"version\": \"0.1\" }"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *suricata_connect(const char *socket_path, int spec)
{
    /*
     * Establish connnection with the Suricata command channel, i.e. used for hostbits, if it exists
     * 
     */
    int command_socket = -1;
    if ((spec & DF_CMD) != DF_CMD)
    {
        return NULL;
    }
    /* connect to the Suricata command socket */
    if ((command_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        return NULL;
    }

    int s;
    char buffer[128];
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path));
    memset(buffer, 0, sizeof(buffer));

    if ((s = connect(command_socket, (struct sockaddr *)&addr, sizeof(addr))) < 0)
    {
        syslog(LOG_ERR, "%s: did not detect %s: %s (%i)\n", __FUNCTION__, addr.sun_path, strerror(errno), errno);
        return NULL;
    }
    syslog(LOG_INFO, "%s: %s", __FUNCTION__, socket_path);

    if ((s = send(command_socket, VERSION_MSG, strlen(VERSION_MSG), 0)) < 0)
    {
        syslog(LOG_ERR, "%s: unable to send socket message: %s\n", __FUNCTION__, strerror(errno));
        return NULL;
    }
    if ((s = read(command_socket, buffer, sizeof(buffer))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to read socket message: %s\n", __FUNCTION__, strerror(errno));
        return NULL;
    }

    DF_HANDLE *dh = (DF_HANDLE *)malloc(sizeof(DF_HANDLE));
    if (!dh)
    {
        syslog(LOG_ERR, "%s: malloc failed", __FUNCTION__);
        return NULL;
    }

    memset(dh, 0, sizeof(DF_HANDLE));
    dh->fd = command_socket;
    dh->io_type = DF_CMD_SURICATA;
    dh->path = strndup(socket_path, PATH_MAX);
    //pthread_mutex_init(&(dh->io_mutex), NULL);
    syslog(LOG_INFO, "%s: %s", __FUNCTION__, socket_path);

    return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int suricata_reconnect(DF_HANDLE *dh)
{

    int s;
    char buffer[128];
    struct sockaddr_un addr;

    close(dh->fd);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, dh->path, sizeof(addr.sun_path));
    memset(buffer, 0, sizeof(buffer));

    if ((dh->fd = connect(dh->fd, (struct sockaddr *)&addr, sizeof(addr))) < 0)
    {
        syslog(LOG_ERR, "%s: did not detect %s: %s (%i)\n", __FUNCTION__, addr.sun_path, strerror(errno), errno);
        return -1;
    }
    syslog(LOG_INFO, "%s: %s", __FUNCTION__, dh->path);

    if ((s = send(dh->fd, VERSION_MSG, strlen(VERSION_MSG), 0)) < 0)
    {
        syslog(LOG_ERR, "%s: unable to send socket message: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    if ((s = read(dh->fd, buffer, sizeof(buffer))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to read socket message: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }

    syslog(LOG_INFO, "%s: %s", __FUNCTION__, dh->path);

    return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int suricata_command(DF_HANDLE *dh, char *command)
{
    int s;
    int len = strnlen(command, DF_MAX_BUFFER_LEN);
    if (len == DF_MAX_BUFFER_LEN)
        return -1;

    do
    {
        s = send(dh->fd, command, len, 0);
        if (s < 0)
        {
            syslog(LOG_ERR, "%s: unable to send command: %s\n", __FUNCTION__, strerror(errno));
            if (s == ECONNRESET) //TODO:  if (n == 0 || n == EIO) ??
            {
                if (suricata_reconnect(dh) < 0)
                {
                    return -1;
                }
            }
            else
                return -1;
        }
    } while (s < 0);

    char response[DF_MAX_BUFFER_LEN];
    memset(response, 0, sizeof(response));
    if ((s = read(dh->fd, response, sizeof(response))) < 0)
    {
        syslog(LOG_ERR, "%s: unable to read response: %s\n", __FUNCTION__, strerror(errno));
    }
    return s;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
#ifdef COMMAND_OUT
int suricata_add_hostbit(const char *time, const char *hostbit_name, const char *iplist, int expire)
{
    int s = -1;
    char *address;
    char command[4096];
    char *list = (char *)iplist;

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
            syslog(LOG_ERR, "%s: unable to send socket message: %s\n", __FUNCTION__, strerror(errno));
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
            syslog(LOG_ERR, "%s: unable to read socket message: %s\n", __FUNCTION__, strerror(errno));
        }
    }
    return 0;
}
#endif
/*
 * ---------------------------------------------------------------------------------------
 */

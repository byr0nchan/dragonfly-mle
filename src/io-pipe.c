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

#include <stdio.h>
#include <stdlib.h>

#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#include <dragonfly-io.h>

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int ipc_reopen(DF_HANDLE *dh)
{
        int s;
        close (dh->fd);
        if ((dh->fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
        {
                syslog(LOG_ERR, "unable to create socket: %s\n", strerror(errno));
                return -1;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, dh->path);

        if (dh->io_type == DF_SERVER_IPC_TYPE)
        {
                unlink(dh->path);
                syslog(LOG_INFO, "Binding to %s\n", dh->path);
#ifdef __DEBUG__
                fprintf(stderr, "%s: Binding to %s (DF_IN)\n", __FUNCTION__,dh->path);
#endif
                if ((s = bind(dh->fd, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to bind socket: %s\n", strerror(errno));
                        return -1;
                }
        }
        else  if (dh->io_type == DF_CLIENT_IPC_TYPE)
        {
                syslog(LOG_INFO, "Connecting to %s\n", dh->path);
#ifdef __DEBUG__
                fprintf(stderr, "%s: Connecting to %s (DF_OUT)\n", __FUNCTION__, dh->path);
#endif
                if ((s = connect(dh->fd, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to connect socket: %s\n", strerror(errno));
                        return -1;
                }
        }

        syslog(LOG_INFO, "%s: %s", __FUNCTION__, addr.sun_path);

        return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *ipc_open(const char *ipc_path, int spec)
{
        int socket_handle = -1;
        int io_type = -1;

        if ((socket_handle = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
        {
                syslog(LOG_ERR, "unable to create socket: %s\n", strerror(errno));
                return NULL;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        if (*ipc_path == '/')
        {
                strcpy(addr.sun_path, ipc_path);
        }
        else
        {
                /* default ipc directory */
                sprintf(addr.sun_path, "%s/%s", RUN_DIR, ipc_path);
        }

        int s;
        if ((spec & DF_IN) == DF_IN)
        {
                unlink(addr.sun_path);
                io_type = DF_SERVER_IPC_TYPE;
                syslog(LOG_INFO, "Binding to %s\n", addr.sun_path);
#ifdef __DEBUG__
                fprintf(stderr, "%s: Binding to %s (DF_IN)\n", __FUNCTION__, addr.sun_path);
#endif
                if ((s = bind(socket_handle, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to bind socket: %s\n", strerror(errno));
                        return NULL;
                }
        }
        else if ((spec & DF_OUT) == DF_OUT)
        {
                io_type = DF_CLIENT_IPC_TYPE;
                syslog(LOG_INFO, "Connecting to %s\n", addr.sun_path);
#ifdef __DEBUG__
                fprintf(stderr, "%s: Connecting to %s (DF_OUT)\n", __FUNCTION__, addr.sun_path);
#endif
                if ((s = connect(socket_handle, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to connect socket: %s\n", strerror(errno));
                        return NULL;
                }
        }
        else
        {
                syslog(LOG_ERR, "%s: invalid file spec  %s", __FUNCTION__, addr.sun_path);
                return NULL;
        }
        DF_HANDLE *dh = (DF_HANDLE *)malloc(sizeof(DF_HANDLE));
        if (!dh)
        {
                syslog(LOG_ERR, "%s: malloc failed", __FUNCTION__);
                return NULL;
        }
        memset(dh, 0, sizeof(DF_HANDLE));

        dh->fd = socket_handle;
        dh->io_type = io_type;
        dh->path = strndup(addr.sun_path, PATH_MAX);
        pthread_mutex_init(&(dh->io_mutex), NULL);
        syslog(LOG_INFO, "%s: %s", __FUNCTION__, dh->path);

        return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int ipc_read_message(DF_HANDLE *dh, char *buffer, int len)
{
        pthread_mutex_lock(&(dh->io_mutex));
        int n = read(dh->fd, buffer, len);
        pthread_mutex_unlock(&(dh->io_mutex));
        if (n < 0)
        {
                syslog(LOG_ERR, "read error: %s", strerror(errno));
        }
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int ipc_write_message(DF_HANDLE *dh, char *buffer)
{
        int len = strnlen(buffer, DF_MAX_BUFFER_LEN);
        if (len == DF_MAX_BUFFER_LEN)
                return -1;

        pthread_mutex_lock(&(dh->io_mutex));
        int n = send(dh->fd, buffer, len, 0);
        pthread_mutex_unlock(&(dh->io_mutex));
        if (n < 0)
        {
                syslog(LOG_ERR, "write error: %s", strerror(errno));
                if (n == 0 || n == EIO)
                {
                        ipc_reopen(dh);
                }
        }
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void ipc_close(DF_HANDLE *dh)
{
        pthread_mutex_unlock(&(dh->io_mutex));
        close(dh->fd);
        dh->fd = -1;
        pthread_mutex_unlock(&(dh->io_mutex));
}

/*
 * ---------------------------------------------------------------------------------------
 */

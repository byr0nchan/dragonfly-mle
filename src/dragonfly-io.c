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
#include <fcntl.h>

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

#include <dragonfly-io.h>

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static DF_HANDLE *ipc_open(const char *path, int spec)
{
        int socket_handle = -1;
        int io_type = -1;
        if ((socket_handle = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
        {
                syslog(LOG_ERR, "unable to create socket: %s\n", strerror(errno));
                return NULL;
        }
        int s;
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path, strnlen(path, PATH_MAX - 1));
        unlink(addr.sun_path);

        if (spec && DF_IN)
        {
                io_type = DF_SERVER_IPC_TYPE;
                syslog(LOG_INFO, "Binding to %s\n", addr.sun_path);
                if ((s = bind(socket_handle, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to bind socket: %s\n", strerror(errno));
                        return NULL;
                }
        }
        else if (spec && DF_OUT)
        {
                io_type = DF_CLIENT_IPC_TYPE;
                syslog(LOG_INFO, "Connecting to %s\n", addr.sun_path);
                if ((s = connect(socket_handle, (struct sockaddr *)&addr, sizeof(addr))) < 0)
                {
                        syslog(LOG_ERR, "unable to connect socket: %s\n", strerror(errno));
                        return NULL;
                }
        }
        else
        {
                syslog(LOG_ERR, "%s: invalid file spec  %s", __FUNCTION__, path);
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
        dh->path = strndup(path, PATH_MAX);
        pthread_mutex_init(&(dh->io_mutex), NULL);
        syslog(LOG_INFO, "%s: %s", __FUNCTION__, path);

        return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int ipc_read_message(DF_HANDLE *dh, char *buffer, int len)
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
static int ipc_write_message(DF_HANDLE *dh, char *buffer)
{
        int len = strnlen(buffer, DF_MAX_BUFFER_LEN);
        if (len == DF_MAX_BUFFER_LEN)
                return -1;

        pthread_mutex_lock(&(dh->io_mutex));
        int n = write(dh->fd, buffer, len);
        pthread_mutex_unlock(&(dh->io_mutex));
        if (n < 0)
        {
                syslog(LOG_ERR, "write error: %s", strerror(errno));
        }
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void ipc_close(DF_HANDLE *dh)
{
        pthread_mutex_unlock(&(dh->io_mutex));
        close(dh->fd);
        dh->fd = -1;
        pthread_mutex_unlock(&(dh->io_mutex));
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static DF_HANDLE *file_open(const char *path, int spec)
{
        FILE *fp = NULL;
        int io_type;
        if (spec && DF_IN)
        {
                io_type = DF_IN_FILE_TYPE;
                if ((fp = fopen(path, "r")) == NULL)
                {
                        syslog(LOG_ERR, "unable to open: %s - %s\n", path, strerror(errno));
                        return NULL;
                }
        }
        else if (spec && DF_OUT)
        {
                io_type = DF_OUT_FILE_TYPE;
                if ((fp = fopen(path, "a")) == NULL)
                {
                        syslog(LOG_ERR, "unable to open: %s - %s\n", path, strerror(errno));
                        return NULL;
                }
        }
        else
        {
                syslog(LOG_ERR, "%s: invalid file spec", __FUNCTION__);
                return NULL;
        }

        DF_HANDLE *dh = (DF_HANDLE *)malloc(sizeof(DF_HANDLE));
        if (!dh)
        {
                syslog(LOG_ERR, "%s: malloc failed", __FUNCTION__);
                return NULL;
        }

        memset(dh, 0, sizeof(DF_HANDLE));
        dh->fp = fp;
        dh->io_type = io_type;
        dh->path = strndup(path, PATH_MAX);
        pthread_mutex_init(&(dh->io_mutex), NULL);
        syslog(LOG_INFO, "%s: %s", __FUNCTION__, path);

        return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int file_rotate(DF_HANDLE *dh)
{
        int status = -1;
        FILE *fp = NULL;
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        char new_path[PATH_MAX];
        snprintf(new_path, PATH_MAX, "%s.%d%d%d.%d%d%d", dh->path,
                 tm.tm_year + 1900,
                 tm.tm_mon + 1,
                 tm.tm_mday,
                 tm.tm_hour,
                 tm.tm_min,
                 tm.tm_sec);

        pthread_mutex_lock(&(dh->io_mutex));
        if (dh->io_type && DF_IN)
        {
                fclose(dh->fp);
                dh->fp = NULL;
                rename(dh->path, new_path);
                syslog(LOG_INFO, "rotated file: %s", dh->path);
                if ((fp = fopen(dh->path, "r")) != NULL)
                {
                        dh->fp = fp;
                        status = 0;
                }
        }
        pthread_mutex_unlock(&(dh->io_mutex));

        return status;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int file_read_line(DF_HANDLE *dh, char *buffer, int max)
{
        pthread_mutex_lock(&(dh->io_mutex));
        char *s = fgets(buffer, max, dh->fp);
        pthread_mutex_unlock(&(dh->io_mutex));

        if (s != NULL)
        {
                int n = strnlen(buffer, max);
                buffer[n - 1] = '\0';
                return n;
        }
        return 0;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int file_write_line(DF_HANDLE *dh, char *buffer)
{
        pthread_mutex_lock(&(dh->io_mutex));
        int n = fputs(buffer, dh->fp);
        pthread_mutex_unlock(&(dh->io_mutex));
        if (n < 0)
        {
                syslog(LOG_ERR, "write error: %s", strerror(errno));
        }
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static void file_close(DF_HANDLE *dh)
{
        pthread_mutex_lock(&(dh->io_mutex));
        fclose(dh->fp);
        dh->fp = NULL;
        pthread_mutex_unlock(&(dh->io_mutex));
}
/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *dragonfly_io_open(const char *uri, int spec)
{
        if (strncmp("file", uri, 4) == 0)
        {
                return file_open(((const char *)uri + 4), spec);
        }
        else if (strncmp("ipc", uri, 3) == 0)
        {
                return ipc_open(((const char *)uri + 3), spec);
        }
        return NULL;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_io_write(DF_HANDLE *dh, char *buffer)
{
        if (!dh)
                return -1;
        if ((dh->io_type == DF_SERVER_IPC_TYPE) ||
            (dh->io_type == DF_CLIENT_IPC_TYPE))
        {
                return ipc_write_message(dh, buffer);
        }
        else if ((dh->io_type == DF_IN_FILE_TYPE) ||
                 (dh->io_type == DF_OUT_FILE_TYPE))
        {
                return file_write_line(dh, buffer);
        }
        return -1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int dragonfly_io_read(DF_HANDLE *dh, char *buffer, int len)
{
        if (!dh)
                return -1;
        if ((dh->io_type == DF_SERVER_IPC_TYPE) ||
            (dh->io_type == DF_CLIENT_IPC_TYPE))
        {
                return ipc_read_message(dh, buffer, len);
        }
        else if ((dh->io_type == DF_IN_FILE_TYPE) ||
                 (dh->io_type == DF_OUT_FILE_TYPE))
        {
                return file_read_line(dh, buffer, len);
        }
        return -1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void dragonfly_io_flush(DF_HANDLE *dh)
{
        if (!dh)
                return;

        if ((dh->io_type == DF_IN_FILE_TYPE) ||
            (dh->io_type == DF_OUT_FILE_TYPE))
        {
                fflush(dh->fp);
        }
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void dragonfly_io_close(DF_HANDLE *dh)
{
        if (!dh)
                return;
        if ((dh->io_type == DF_SERVER_IPC_TYPE) ||
            (dh->io_type == DF_CLIENT_IPC_TYPE))
        {
                ipc_close(dh);
        }
        else if ((dh->io_type == DF_IN_FILE_TYPE) ||
                 (dh->io_type == DF_OUT_FILE_TYPE))
        {
                file_close(dh);
        }
        free(dh->path);
        pthread_mutex_destroy(&(dh->io_mutex));
        free(dh);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void dragonfly_io_rotate(DF_HANDLE *dh)
{
        // only output files get rotated
        if (dh && (dh->io_type == DF_OUT_FILE_TYPE))
        {
                file_rotate(dh);
        }
}

/*
 * ---------------------------------------------------------------------------------------
 */

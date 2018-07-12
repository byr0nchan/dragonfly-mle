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
#include <string.h>

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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>
#include <zlib.h>

#include <dragonfly-io.h>

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *zfile_open(const char *ipc_path, int spec)
{
        int io_type = -1;
        char zfile_path[PATH_MAX];
        gzFile zfp;

        if (*ipc_path == '/')
        {
                strcpy(zfile_path, ipc_path);
        }
        else
        {
                /* default ipc directory */
                sprintf(zfile_path, "%s/%s", RUN_DIR, ipc_path);
        }
        if ((spec & DF_IN) == DF_IN)
        {
                //unlink(zfile_path);
                if (access(zfile_path, W_OK) < 0)
                {
                        if (mkfifo(zfile_path, 0666) == -1)
                        {
                                syslog(LOG_ERR, "unable to mkfifo(%s): %s\n", zfile_path, strerror(errno));
                                return NULL;
                        }
                }
                if ((zfp = gzopen(zfile_path, "r")) == NULL)
                {
                        syslog(LOG_ERR, "unable to fopen(%s): %s\n", zfile_path, strerror(errno));
                        return NULL;
                }
                io_type = DF_IN_ZFILE_TYPE;
        }
        else if ((spec & DF_OUT) == DF_OUT)
        {
                if ((zfp = gzopen(zfile_path, "w")) == NULL)
                {
                        syslog(LOG_ERR, "unable to fopen(%s): %s\n", zfile_path, strerror(errno));
                        return NULL;
                }
                io_type = DF_OUT_ZFILE_TYPE;
        }
        else
        {
                syslog(LOG_ERR, "%s: invalid file spec  %s", __FUNCTION__, zfile_path);
                return NULL;
        }
        DF_HANDLE *dh = (DF_HANDLE *)malloc(sizeof(DF_HANDLE));
        if (!dh)
        {
                syslog(LOG_ERR, "%s: malloc failed", __FUNCTION__);
                return NULL;
        }
        memset(dh, 0, sizeof(DF_HANDLE));

        dh->zfp = zfp;
        dh->io_type = io_type;
        dh->path = strndup(zfile_path, PATH_MAX);
#ifdef __DEBUG3__
        syslog(LOG_INFO, "%s: %s", __FUNCTION__, path);
#endif
        return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * 
 * ---------------------------------------------------------------------------------------
 */
int zfile_read_line(DF_HANDLE *dh, char *buffer, int len)
{
        if (gzgets(dh->zfp, buffer, len) == Z_NULL)
        {
                if (gzeof(dh->zfp))
                        return 0;
                int error;
                syslog(LOG_ERR, "%s: error -- %s", __FUNCTION__, gzerror(dh->zfp, &error));
                return -1;
        }
        int i = strnlen(buffer, len);
        // remove '\n'
        buffer[i - 1] = '\0';
        return (i - 1);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int zfile_write_line(DF_HANDLE *dh, char *buffer)
{
        int n = gzputs(dh->zfp, buffer);
        if (n < 0)
        {
                int error;
                syslog(LOG_ERR, "%s: error -- %s", __FUNCTION__, gzerror(dh->zfp, &error));
                return n;
        }
        if (gzputc(dh->zfp, '\n') < 0)
        {
                return -1;
        }
        return (n + 1);
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int zfile_rotate(DF_HANDLE *dh)
{
        int status = -1;
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

        if ((dh->io_type & DF_OUT) == DF_OUT)
        {
                gzclose(dh->zfp);
                dh->zfp = NULL;
                rename(dh->path, new_path);
                syslog(LOG_INFO, "rotated file: %s -> %s", dh->path, new_path);
                if ((dh->zfp = gzopen(dh->path, "w")) != NULL)
                {
                        int error;
                        syslog(LOG_ERR, "%s: error -- %s", __FUNCTION__, gzerror(dh->zfp, &error));
                }
                else
                {
                        status = 0;
                }
        }

        return status;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void zfile_close(DF_HANDLE *dh)
{
        if (dh && dh->zfp)
        {
                gzclose(dh->zfp);
        }
}

/*
 * ---------------------------------------------------------------------------------------
 */

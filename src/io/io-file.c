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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>

#include "dragonfly-io.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *file_open(const char *path, int spec)
{
        FILE *fp = NULL;
        int io_type;
        char file_path[PATH_MAX];

        if (*path == '/')
        {
                strncpy(file_path, path, PATH_MAX);
        }
        else
        {
                /* default ipc directory */
                snprintf(file_path, PATH_MAX, "%s/%s", LOG_DIR, path);
        }

        if ((spec & DF_IN) == DF_IN)
        {
#ifdef __DEBUG3__
                fprintf(stderr, "%s: %s (DF_IN)\n", __FUNCTION__, path);
#endif

                io_type = DF_IN_FILE_TYPE;
                if ((fp = fopen(file_path, "r")) == NULL)
                {
                        syslog(LOG_ERR, "unable to open: [%s] - %s\n", path, strerror(errno));
                        return NULL;
                }
        }
        else if ((spec & DF_OUT) == DF_OUT)
        {
                char mode[8];

                int last = strnlen(file_path, PATH_MAX);
                if (last > 0)
                        last--;
                if (file_path[last] == '<')
                {
                        file_path[last] = '\0';
                        strcpy(mode, "w+");
                }
                else if (file_path[last] == '>')
                {
                        file_path[last] = '\0';
                        strcpy(mode, "a+");
                }
                else
                {
                        strcpy(mode, "a+");
                }
#ifdef __DEBUG3__
                fprintf(stderr, "%s: [%s] (DF_OUT)\n", __FUNCTION__, path);
#endif
                io_type = DF_OUT_FILE_TYPE;
                if ((fp = fopen(file_path, mode)) == NULL)
                {
                        syslog(LOG_ERR, "unable to open: %s - %s\n", path, strerror(errno));
                        return NULL;
                }
                setvbuf(fp, NULL, _IOLBF, 0);
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
        dh->path = strndup(file_path, PATH_MAX);
#ifdef __DEBUG3__
        syslog(LOG_INFO, "%s: %s", __FUNCTION__, path);
#endif
        return dh;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int file_rotate(DF_HANDLE *dh)
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

        if ((dh->io_type & DF_OUT) == DF_OUT)
        {
                fclose(dh->fp);
                dh->fp = NULL;
                rename(dh->path, new_path);
                syslog(LOG_INFO, "rotated file: %s -> %s", dh->path, new_path);
                if ((fp = fopen(dh->path, "a")) != NULL)
                {
                        dh->fp = fp;
                        setlinebuf(fp);
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
int file_read_line(DF_HANDLE *dh, char *buffer, int max)
{
        char *s = fgets(buffer, max, dh->fp);

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
int file_write_line(DF_HANDLE *dh, char *buffer)
{
        int n = fputs(buffer, dh->fp);
        if (n < 0)
        {
#ifdef __DEBUG__
                fprintf(stderr, "%s: line %d: %s\n", __FUNCTION__, __LINE__, strerror(errno));
#endif
                syslog(LOG_ERR, "write error: %s", strerror(errno));
                if (n == 0 || n == EIO)
                {
                        //file_reopen(dh);
                }
        }
        fputs("\n", dh->fp);
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void file_close(DF_HANDLE *dh)
{
        if (dh && dh->fp)
        {
                fflush(dh->fp);
                fclose(dh->fp);
                dh->fp = NULL;
        }
}

/*
 * ---------------------------------------------------------------------------------------
 */

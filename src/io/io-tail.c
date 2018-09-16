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
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

#include "dragonfly-io.h"
#include "config.h"

#define TEN_SECONDS (10)
#define FIVE_SECONDS (5)

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
static int tail_open_nonblock_file(const char *path)
{
        char buffer[PATH_MAX];
        strncpy(buffer, path, PATH_MAX);
        int whence = strnlen(buffer, PATH_MAX);
        if (whence > 0)
                whence--;
        if (buffer[whence] == '<')
        {
                buffer[whence] = '\0';
                whence = SEEK_SET;
        }
        else if (buffer[whence] == '>')
        {
                buffer[whence] = '\0';
                whence = SEEK_END;
        }
        else
        {
                whence = SEEK_END;
        }

        int fd = open(buffer, O_RDONLY);
        if (fd < 0)
        {

                syslog(LOG_ERR, "unable to tail open: %s - %s\n", buffer, strerror(errno));
                return -1;
        }
        if (lseek(fd, 0, whence) < 0)
        {
                close(fd);
                syslog(LOG_ERR, "unable to seek: %s - %s\n", buffer, strerror(errno));
                return -1;
        }
        int fd_flags = fcntl(fd, F_GETFL);
        if (fd_flags < 0)
        {
                close(fd);
                syslog(LOG_ERR, "unable to fcntl: %s - %s\n", buffer, strerror(errno));
                return -1;
        }
        if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) < 0)
        {
                close(fd);
                syslog(LOG_ERR, "unable to fcntl: %s - %s\n", buffer, strerror(errno));
                return -1;
        }
        return fd;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *tail_open(const char *path, int spec)
{
        int fd = -1;
        int io_type;
        char file_path[PATH_MAX];

        if (*path == '/')
        {
                strncpy(file_path, path, PATH_MAX);
        }
        else
        {
                /* default log directory */
                snprintf(file_path, PATH_MAX, "%s/%s", dragonfly_io_get_logdir(), path);
        }

        if ((spec & DF_IN) == DF_IN)
        {
                io_type = DF_IN_TAIL_TYPE;
                if ((fd = tail_open_nonblock_file(file_path)) < 0)
                {
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
        dh->fd = fd;
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

static int tail_next_line(DF_HANDLE *dh, char *buffer, int len)
{
        time_t last_read_time = time(NULL);
        time_t current_read_time = 0;
        int sleep_backoff = 1;
        int open_failures = 0;
        long long lastFileSize = 0;
        struct stat fdstat;
        int end_of_line = 0;
        int i = 0;
        do
        {
                if (dh->fd < 0)
                {
                        dh->fd = tail_open_nonblock_file(dh->path);
                        if (dh->fd < 0)
                        {
                                open_failures++;
                                if (open_failures > 10)
                                {
                                        return -1;
                                }
                                sleep(1);
                                continue;
                        }
                }
                int n = read(dh->fd, &buffer[i], 1);
                if (n < 0)
                {
                        if (n == EAGAIN)
                        {
                                sleep(1);
                                continue;
                        }
                        else
                        {
                                syslog(LOG_ERR, "unable to read: %s\n", strerror(errno));
                                return -1;
                        }
                }
                else if (n == 0)
                {
                        if (fstat(dh->fd, &fdstat) < 0)
                        {
                                syslog(LOG_ERR, "unable to fstat: %s\n", strerror(errno));
                        }
#ifdef __DEBUG3__
                        fprintf(stderr, "file size: %lld bytes\n", (long long)fdstat.st_size);
#endif
                        if (fdstat.st_size < lastFileSize)
                        {
#ifdef __DEBUG3__
                                fprintf(stderr, "file was truncated\n");
#endif
                                lseek(dh->fd, 0, SEEK_SET);
                                i = 0;
                        }
                        lastFileSize = fdstat.st_size;
                        if (sleep_backoff < FIVE_SECONDS)
                        {
                                sleep_backoff++;
                        }
                        current_read_time = time(NULL);
                        if ((current_read_time - last_read_time) >= TEN_SECONDS)
                        {
                                struct stat fdstat2;
                                last_read_time = current_read_time;
                                stat(dh->path, &fdstat2);
                                if (fdstat.st_ino != fdstat2.st_ino)
                                {
                                        // file was moved/renamed
#ifdef __DEBUG3__
                                        fprintf(stderr, "%s: file moved or renamed\n", __FUNCTION__);
#endif
                                        close(dh->fd);
                                        char file_spec[PATH_MAX];
                                        snprintf(file_spec, sizeof(file_spec), "%s<", dh->path);
                                        if ((dh->fd = tail_open_nonblock_file(file_spec)) < 0)
                                        {
                                                return -1;
                                        }
                                        //fprintf(stderr, "%s: file reopened\n", __FUNCTION__);
                                        i = 0;
                                }
                        }
                        sleep(sleep_backoff);
                }
                else if (buffer[i] == '\n')
                {
                        end_of_line = 1;
                        //fprintf (stderr,"%s: %lu\n", __FUNCTION__, g_counter++);
                }
                else
                {
                        i++;
                }
        } while (!end_of_line && (i < len));
        buffer[i] = '\0';
        return i;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int tail_read_line(DF_HANDLE *dh, char *buffer, int max)
{
        int n = 0;
        do
        {
                n = tail_next_line(dh, buffer, max);
                // when n == 0, then skip to the next line
                // because n ==0 is an empty string with just "\n"
        } while (n == 0);
        return n;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void tail_close(DF_HANDLE *dh)
{
#ifdef __DEBUG3__
        fprintf(stderr, "%s: line %d\n", __FUNCTION__, __LINE__);
#endif
        if (dh)
        {
                close(dh->fd);
                dh->fd = -1;
        }
}

/*
 * ---------------------------------------------------------------------------------------
 */

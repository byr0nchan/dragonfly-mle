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
#include <syslog.h>

#include "dragonfly-io.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *syslog_open(const char *path, int spec)
{
        int facility = 0;
        int io_type = 0;
        if ((spec & DF_OUT) == DF_OUT)
        {
#ifdef __DEBUG3__
                fprintf(stderr, "%s: %s (DF_OUT)\n", __FUNCTION__, path);
#endif
                if (path[0]=='L' && path[1]=='O' && path[2]=='G' && path[3]=='_' &&
                    path[4]=='L' && path[5]=='O' && path[6]=='C' && path[7]=='A' && path[8]=='L')
                {
                        switch (atoi(&path[9]))
                        {
                                case 0: facility = LOG_LOCAL0; break;
                                case 1: facility = LOG_LOCAL1; break;
                                case 2: facility = LOG_LOCAL2; break;
                                case 3: facility = LOG_LOCAL3; break;
                                case 4: facility = LOG_LOCAL4; break;
                                case 5: facility = LOG_LOCAL5; break;
                                case 6: facility = LOG_LOCAL6; break;
                                case 7: facility = LOG_LOCAL7; break;
                                default: 
                                        syslog(LOG_ERR, "unrecongized LOG_LOCAL type: %s", path);
                                        return NULL;
                        }
                };
                io_type = DF_OUT_SYSLOG_TYPE;
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
        dh->fd = facility;
        dh->io_type = io_type;
        dh->path = strndup(path, PATH_MAX);
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
int syslog_write_message(DF_HANDLE *dh, char *buffer)
{
        syslog(dh->fd,"%s", buffer);
        return 1;
}

/*
 * ---------------------------------------------------------------------------------------
 */

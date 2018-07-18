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

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *cron_open(const char *interval, int spec)
{
        int io_type;

        if ((spec & DF_IN) == DF_IN)
        {
                io_type = DF_IN_CRON_TYPE;
        }
        else
        {
                syslog(LOG_ERR, "%s: invalid cron spec", __FUNCTION__);
                return NULL;
        }
        int minutes = atoi(interval);
        if (minutes<=0 || minutes >= 1440)
        {
                syslog(LOG_ERR, "%s: minutes must be greater than 0 and less than 1440 (24 hours)", __FUNCTION__);
                return NULL;  
        }
        DF_HANDLE *dh = (DF_HANDLE *)malloc(sizeof(DF_HANDLE));
        if (!dh)
        {
                syslog(LOG_ERR, "%s: malloc failed", __FUNCTION__);
                return NULL;
        }
        memset(dh, 0, sizeof(DF_HANDLE));
        dh->interval = (minutes * 60);
        dh->epoch = (int)time(NULL) + dh->interval;
        dh->io_type = io_type;
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
int cron_push(DF_HANDLE *dh, char *buffer, int max)
{
        memset (buffer, 0, max);
        int now = 0;
        do
        {
                sleep(1);
                now = (int)time(NULL);
        } while (now < dh->epoch);
        dh->epoch += (dh->interval*60);
        return now;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int cron_poll(DF_HANDLE *dh, char *buffer, int max)
{
        memset (buffer, 0, max);
        int now = 0;
        do
        {
                sleep(1);
                now = (int)time(NULL);
        } while (now < dh->epoch);
        dh->epoch += (dh->interval*60);
        return now;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void cron_close(DF_HANDLE *dh)
{
#ifdef __DEBUG3__
        fprintf(stderr, "%s: line %d\n", __FUNCTION__, __LINE__);
#endif
}
/*
 * ---------------------------------------------------------------------------------------
 */

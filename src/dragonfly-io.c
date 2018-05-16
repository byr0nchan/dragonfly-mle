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
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <errno.h>

#include "dragonfly-io.h"

#include "io-file.h"
#include "io-tail.h"
#include "io-pipe.h"

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
DF_HANDLE *dragonfly_io_open(const char *uri, int spec)
{
        if (strncmp("file://", uri, 7) == 0)
        {
                return file_open(((const char *)uri + 7), spec);
        }
        else if (strncmp("tail://", uri, 7) == 0)
        {
                return tail_open(((const char *)uri + 7), spec);
        }
        else if (strncmp("ipc://", uri, 6) == 0)
        {
                return ipc_open(((const char *)uri + 6), spec);
        }
        else if (strncmp("suricata://", uri, 11) == 0)
        {
                return ipc_open(((const char *)uri + 11), spec);
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
        else if (dh->io_type == DF_OUT_FILE_TYPE)
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
        else if (dh->io_type == DF_IN_TAIL_TYPE)
        {
                return tail_read_line(dh, buffer, len);
        }
        else if (dh->io_type == DF_IN_FILE_TYPE)
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
int dragonfly_io_read_lines(DF_HANDLE *dh, char **buffer, int len, int max)
{
        if (!dh)
                return -1;

        if ((dh->io_type == DF_SERVER_IPC_TYPE) ||
            (dh->io_type == DF_CLIENT_IPC_TYPE))
        {
                return ipc_read_messages(dh, buffer, len, max);
        }
        /*
         * TODO: add multi-line read (vector) support for the types below
         * 
        else if (dh->io_type == DF_IN_TAIL_TYPE)
        {
                return tail_read_line(dh, buffer, len);
        }
        else if (dh->io_type == DF_IN_FILE_TYPE)
        {
                return file_read_line(dh, buffer, len);
        }*/
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

        if (dh->io_type == DF_OUT_FILE_TYPE)
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
        else if (dh->io_type == DF_IN_TAIL_TYPE)
        {
                tail_close(dh);
        }
        else if ((dh->io_type == DF_IN_FILE_TYPE) ||
                 (dh->io_type == DF_OUT_FILE_TYPE))
        {
                file_close(dh);
        }
        free(dh->path);
        dh->path = NULL;
        free(dh);
        dh = NULL;
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

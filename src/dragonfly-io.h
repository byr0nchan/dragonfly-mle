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

#ifndef _DRAGONFLY_IO_H_
#define _DRAGONFLY_IO_H_

#define RUN_DIR "run"
#define LOG_DIR "log"

#define DF_IN 1
#define DF_OUT 2
#define DF_ERR 4
#define DF_CMD 8

#define DF_OUT_SYSLOG_TYPE 0
#define DF_IN_FILE_TYPE 1
#define DF_OUT_FILE_TYPE 2
#define DF_CLIENT_IPC_TYPE  3
#define DF_SERVER_IPC_TYPE  4
#define DF_IN_TAIL_TYPE 5
#define DF_IN_KAFKA_TYPE 6
#define DF_OUT_KAFKA_TYPE 7
#define DF_CMD_SURICATA 8

#define DF_MAX_BUFFER_LEN 2048

typedef struct _DF_HANDLE_
{
    FILE *fp;
    int fd;
    int io_type;
    char *path;
    void *ptr;
} DF_HANDLE;

DF_HANDLE *dragonfly_io_open(const char *path, int spec);
int dragonfly_io_write(DF_HANDLE *dh, char *buffer);
int dragonfly_io_read(DF_HANDLE *dh, char *buffer, int max);
int dragonfly_io_read_lines(DF_HANDLE *dh, char *buffer[], int len, int max);
void dragonfly_io_flush(DF_HANDLE *dh);
void dragonfly_io_close(DF_HANDLE *dh);
void dragonfly_io_rotate(DF_HANDLE *dh);

#endif


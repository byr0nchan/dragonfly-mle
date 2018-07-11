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

#ifndef _IO_ZPIPE_H_
#define _IO_ZPIPE_H_

/*
 * zfile IO
 */
DF_HANDLE *zfile_open(const char *path, int spec);
int zfile_write_line(DF_HANDLE *dh, char *buffer);
int zfile_read_line(DF_HANDLE *dh, char *buffer, int max);
void zfile_flush(DF_HANDLE *dh);
void zfile_close(DF_HANDLE *dh);
void zfile_rotate(DF_HANDLE *dh);

#endif

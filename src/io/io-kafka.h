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

#ifndef _IO_KAFKA_H_
#define _IO_KAFKA_H_

/*
 * Kafka IO
 */

DF_HANDLE *kafka_open(const char *uri, int spec);
int kafka_read_message (DF_HANDLE *dh, char *buffer, int max);
int kafka_write_message(DF_HANDLE *dh, char *buffer);
void kafka_close(DF_HANDLE *dh);

#endif

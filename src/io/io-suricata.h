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

#ifndef _SURICATA_COMMANDS_
#define _SURICATA_COMMANDS_

#include <dragonfly-cmds.h>

DF_HANDLE *suricata_connect(const char *socket_path, int spec);
int suricata_command(DF_HANDLE *dh, char *command);
int suricata_add_hostbit(const char* time, const char* hostbit_name, const char*iplist, int expire);

#endif


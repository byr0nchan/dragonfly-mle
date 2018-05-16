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

#include <pwd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>

#include "param.h"
#include "resp-suricata.h"

typedef struct _RESPONDER_
{
    char *tag;
    int (*pf_initialize)(const char*);
    int (*pf_command)(const char*, char*, int);
} RESPONDER;

static RESPONDER g_responder[MAX_RESPONDER_COMMANDS];

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
void responder_initialize()
{
    memset(g_responder, 0, sizeof(g_responder));
    /*
     * Support for Suricata's command channel
     */
    g_responder[0].tag = "suricata";
    g_responder[0].pf_initialize = suricata_initialize;
    g_responder[0].pf_command = suricata_command;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int responder_setup(const char *tag, const char *parameter)
{
    for (int i = 0; i < MAX_RESPONDER_COMMANDS; i++)
    {
        if (g_responder[i].tag && strcasecmp(tag, g_responder[i].tag) == 0)
        {
            return g_responder[i].pf_initialize(parameter);
        }
    }
    return -1;
}

/*
 * ---------------------------------------------------------------------------------------
 *
 * ---------------------------------------------------------------------------------------
 */
int responder_event(const char *tag, const char *command, char *response, int max_response)
{
    for (int i = 0; i < MAX_RESPONDER_COMMANDS; i++)
    {
        if (g_responder[i].tag && strcasecmp(tag, g_responder[i].tag) == 0)
        {
            return g_responder[i].pf_command(command, response, max_response);
        }
    }
    return -1;
}

/*
 * ---------------------------------------------------------------------------------------
 */
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

#ifndef _SELF_TEST_H_
#define _SELF_TEST_H_


#include "dragonfly-lib.h"
#include "dragonfly-io.h"

#define ANALYZER_TEST_FILE "./analyzer/analyzer.lua"
#define FILTER_TEST_FILE "./filter/filter.lua"
#define CONFIG_TEST_FILE "./config/config.lua"

void dragonfly_mle_test(const char *dragonfly_root);
void SELF_TEST0(const char *dragonfly_root);
void SELF_TEST1(const char *dragonfly_root);
void SELF_TEST2(const char *dragonfly_root);
void SELF_TEST3(const char *dragonfly_root);
void SELF_TEST4(const char *dragonfly_root);
void SELF_TEST5(const char *dragonfly_root);
void SELF_TEST6(const char *dragonfly_root);
void SELF_TEST7(const char *dragonfly_root);
void SELF_TEST8(const char *dragonfly_root);
void SELF_TEST9(const char *dragonfly_root);
void SELF_TEST10(const char *dragonfly_root);
#endif

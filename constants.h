/*
 * Copyright (c) 2015. NB Plus (www.nbplus.co.kr)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _CONSTANTS_H_
#define _CONSTANTS_H_

#include <stdio.h>

#define VERSION "00.01.00"

// message queue command value
#define MQ_CMD_IFACE_URL_CHANGED                0x01
#define MQ_CMD_KEEPALIVE_TIMER_EXPIRED          0x02
#define MQ_CMD_PUSHAGENT_CONN_TERMINATED        0x03

// message queue name
// main 
#define MQ_NAME_MAINQUEUE                       "/pushAgentMainMessageQueue"

// Push Interface server
#define PUSH_IF_CONTEXT                         "/is/api/appRequest/SessionRequest"
#define PUSH_IF_OK                              "0000"

enum LOG_LEVEL_VLAUE {
    VERBOSE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4
};

// debug
#define LOG_LEVEL DEBUG

#define log(level, fmt, ...) \
    do { if (level >= LOG_LEVEL) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, ##__VA_ARGS__); } while(0)

#endif          // _CONSTANTS_H_

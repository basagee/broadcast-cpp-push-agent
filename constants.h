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
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

#define VERSION "00.01.00"

// message queue command value
#define MQ_CMD_IFACE_URL_CHANGED                0x01
#define MQ_CMD_KEEPALIVE_TIMER_EXPIRED          0x02
#define MQ_CMD_PUSHAGENT_CONN_TERMINATED        0x03

// message queue name
// main 
#define MQ_NAME_MAINQUEUE                       "/pushAgentMainMessageQueue"
#define MQ_NAME_MAINQUEUE_SIZE                  26

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
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define log(level, fmt, ...)  do { \
        if (level >= LOG_LEVEL) { \
            time_t     now = time(0); \
            struct tm  tstruct; \
            char       buf[80]; \
            \
            tstruct = *localtime(&now); \
            struct timeval tv; \
            gettimeofday(&tv, NULL); \
            int millisec = lrint(tv.tv_usec/1000.0); \
            if (millisec>=1000) { \
                millisec -=1000; \
                tv.tv_sec++; \
            } \
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tstruct); \
            \
            fprintf(stderr, "[%s.%03d] %s:%d:%s(): " fmt, \
                                buf, millisec,  __FILENAME__, \
                                __LINE__, __func__, ##__VA_ARGS__); \
        } \
    } while(0)

#endif          // _CONSTANTS_H_

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
#ifndef _PUSH_CONNECTION_H_
#define _PUSH_CONNECTION_H_

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "pushgateway.h"

#define TCP_READ_BUFFER_SIZE 4096

enum PushConnError {
    OK = 0,
    CONN_DATA_NOT_FOUND = -1,
    INVALID_SESSION_KEY = -2,
    INVALID_AUTH_KEY = -3,
    UNKNOWN_IP_ADDRESS = -4,
    UNKNOWN_PORT = -5,
    NETWORK_UNAVAILABLE = -6,
    SOCKET_OPEN_ERROR = -7,
    SOCKET_CONNECT_ERROR = -8,
    READ_THREAD_FAILED = -9
};

#define ASCII_MESSAGE_TYPE_BASE                         48
#define CONNECTION_REQUEST_LENGTH                       29
#define KEEP_ALIVE_REQUEST_LENGTH                       13
#define PUSH_MESSAGE_RESPONSE_LENGTH                    17
#define APP_UPDATE_RESPONSE_LENGTH                      13
#define CHAR_SIZE                                       1
#define INT_SIZE                                        4
#define APPID_SIZE                                      100
#define URL_SIZE                                        100
#define REPEAT_KEY_SIZE                                 10
#define ALERT_SIZE                                      300
#define RETRY_TIMER_MS_INTERVAL                         300000
#define NOT_RESPOND_RETRY_TIMER_MS_INTERVAL             5000
#define KEEP_ALIVE_RESPONSE_TIMER_MS_INTERVAL           60000
#define SECONDS_TO_MILLISECONDS                         1000

// push message payload service type definitions
#define SERVICE_TYPE_REALTIME_BROADCAST                 "00"
#define SERVICE_TYPE_NORMAL_BROADCAST                   "01"
#define SERVICE_TYPE_TEXT_BROADCAST                     "02"
#define SERVICE_TYPE_EMERGENCY_CALL                     "03"
#define SERVICE_TYPE_INHABITANTS_POLL                   "04"
#define SERVICE_TYPE_COOPERATIVE_BUYING                 "05"
#define SERVICE_TYPE_IOT_DEVICE_CONTROL                 "06"
#define SERVICE_TYPE_PUSH_NOTIFICATION                  "07"
#define SERVICE_TYPE_FIND_PASSWORD                      "08"

enum PushMessageType {
    UNKNOWN_MESSAGE_TYPE = 0,
    CONNECTION_REQUEST = '0',
    CONNECTION_RESPONSE = '1',
    PUSH_REQUEST = '2',
    PUSH_RESPONSE = '3',
    KEEP_ALIVE_REQUEST = '4',
    KEEP_ALIVE_RESPONSE = '5',
    KEEP_ALIVE_CHANGE_REQUEST = '6',
    KEEP_ALIVE_CHANGE_RESPONSE = '7',
    PUSH_AGENT_UPDATE_REQUEST = '8',
    PUSH_AGENT_UPDATE_RESPONSE = '9',
    APP_UPDATE_REQUEST = 'a',
    APP_UPDATE_RESPONSE = 'b'
};

#define PUSH_MESSAGE_HEADER_LENGHT 9

typedef struct PushMessageHeader_t {   // Declare PERSON struct type
    PushMessageType type;   // Declare member types
    int messageId;
    int bodyLen;
    int restChunkLen;
} PushMessageHeader;   // Define object of type PERSON

class PushConnection {
    public:
        timer_t keepAliveTimerId;
        timer_t keepAliveRespTimerId;
        timer_t retryConnectionTimerId;

        PushConnection(/*char * deviceId*/);
        PushConnection(/*char * deviceId, */PushConnData *connData);
        ~PushConnection();
        
        void setPushConnectionData(PushConnData *connData);
        int startPushAgent();
        int startPushAgent(PushConnData *connData);
        int stopPushAgent();
        static void retryTimerFunc(int sig, siginfo_t *si, void *uc);
        static void keepAliveTimerFunc(int sig, siginfo_t *si, void *uc);
        static void keepAliveResponseTimerFunc(int sig, siginfo_t *si, void *uc);
        static void readThreadSignalHandler(int signum);
        int getKeepAliveTimeSeconds() { if (connData != NULL) return connData->getKeepAlivePeriod(); else return 0; }
        int isAlive() { return status; }
        
    protected:
        int getSockfd() { return sockfd; }
        void handlePushMessageFromServer(PushMessageType type, int messageId, int bodyLen, char *body);
        PushMessageHeader parseMessageHeader(char *message);
        int sendRequestOrResponseToServer(PushMessageType type);
        int sendRequestOrResponseToServer(PushMessageType type, int messageId, int correlator);
    
    private:
        PushConnData* connData;
        int sockfd;
        pthread_t readThread;  
        pthread_attr_t attr;
        char *deviceId;
        int requestMessageId;
        int status;
        
        int connectToServer();
        int runReadThread();
        static void* ReadThreadFunc(void *);
        int ntohlFromCharArray(char *arr);
        
};

#endif                  // _PUSH_CONNECTION_H_

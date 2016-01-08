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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>  
#include <signal.h>  
#include <errno.h>
#include "constants.h"
#include "pushconnection.h"
#include "utils.h"

#include <sys/time.h>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"

using namespace std;
using namespace rapidjson;

PushConnection::PushConnection(/*char* deviceId*/) { 
    this->requestMessageId = 0;
    this->deviceId = deviceId;
    this->keepAliveTimerId = 0;
    this->keepAliveRespTimerId = 0;
    this->readThread = 0;
    this->sockfd = 0;
    this->connData = NULL;
    this->retryConnectionTimerId = 0;
    this->status = 0;
}

PushConnection::PushConnection(/*char* deviceId, */PushConnData* connData) { 
    /*this->deviceId = deviceId;*/
    this->requestMessageId = 0;
    this->connData = connData; 
    this->keepAliveTimerId = 0;
    this->keepAliveRespTimerId = 0;
    this->readThread = 0;
    this->sockfd = 0;
    this->retryConnectionTimerId = 0;
    
    this->status = 0;
}

PushConnection::~PushConnection() { 
    if (this->connData != NULL) {
        delete this->connData;
    }
    this->connData = NULL;
}

void PushConnection::setPushConnectionData(PushConnData* connData) {
    this->connData = connData;
}

int PushConnection::startPushAgent() {
    startPushAgent(this->connData);
}

int PushConnection::startPushAgent(PushConnData* connData) {
    this->connData = connData; 
    
    if (this->connData == NULL) {
        return CONN_DATA_NOT_FOUND;
    }
    
    if (this->connData->getAuthKey() == NULL ||
        strlen(this->connData->getAuthKey()) == 0) {
        return INVALID_AUTH_KEY;
    }
    if (this->connData->getAuthKey() == NULL ||
        strlen(this->connData->getAuthKey()) == 0) {
        return INVALID_AUTH_KEY;
    }
    if (this->connData->getSessionKey() == NULL ||
        strlen(this->connData->getSessionKey()) == 0) {
        return INVALID_SESSION_KEY;
    }
    if (this->connData->getGwIpAddr() == NULL ||
        strlen(this->connData->getGwIpAddr()) == 0) {
        return UNKNOWN_IP_ADDRESS;
    }
    if (this->connData->getGwPort() <= 0) {
        return UNKNOWN_PORT;
    }
    
    int errorCode = OK;
    if ((errorCode = connectToServer()) == OK) {
        log(DEBUG, "Push agent connection success.\n");
        
        this->status = 1;
        return OK;
    } else {
        return errorCode;
    }
}

int PushConnection::connectToServer() {
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int retCode = OK;
    
    if (Utils::checkNetworkStatus() != 1) {
        log(ERROR, "NETWORK_UNAVAILABLE\n");
        return NETWORK_UNAVAILABLE;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log(ERROR, "Error opening socket.\n");
        return SOCKET_OPEN_ERROR;
    }
    
    server = gethostbyname(this->connData->getGwIpAddr());
    if (server == NULL) {
        log(ERROR, "Error, no such host\n");
        return UNKNOWN_IP_ADDRESS;
    }
    
    bzero((char *)&serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr, server->h_length);
    serv_addr.sin_port = htons(this->connData->getGwPort());
    
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log(ERROR, "Error, connection to push agent server\n");
        return SOCKET_CONNECT_ERROR;
    }
    
    // create read thread. 
    if ((retCode = runReadThread()) != OK) {
        return retCode;
    }
    
    // wait second.
    sleep(1);
    
    // send connection request 
    sendRequestOrResponseToServer(CONNECTION_REQUEST);
    
    return OK;
}


int PushConnection::stopPushAgent() {
    log(DEBUG, "stopPushAgent called.\n");
    if (this->readThread > 0) {
        // i/o blocking 되어 있다. 
        /*pthread_cancel*/pthread_kill(this->readThread, SIGUSR1);
        pthread_join(this->readThread, NULL);
        pthread_attr_destroy(&attr);
        log(DEBUG, "Pthread killed\n");
    }
    
    if (sockfd > 0) {
        close(sockfd);
    }
    
    // stop keep-alive timer
    if (this->keepAliveTimerId > 0) {
        Utils::stopTimer(this->keepAliveTimerId);
    }
    if (this->keepAliveRespTimerId > 0) {
        Utils::stopTimer(this->keepAliveRespTimerId);
    }
    if (this->retryConnectionTimerId > 0) {
        Utils::stopTimer(this->retryConnectionTimerId);
    }
    this->keepAliveTimerId = 0;
    this->keepAliveRespTimerId = 0;
    this->readThread = 0;
    this->sockfd = 0;
    this->retryConnectionTimerId = 0;
    this->status = 0;
}

// Send data to server function
int PushConnection::sendRequestOrResponseToServer(PushMessageType type) {
    return sendRequestOrResponseToServer(type, -1, -1);
}

int PushConnection::sendRequestOrResponseToServer(PushMessageType type, int messageId, int correlator) {
    char *sendBuffer = NULL;
    char *writePos = NULL;
    int  sendLen = 0;
    int  htonlVal = 0;
    
    if (requestMessageId >= INT_MAX) {
        requestMessageId = 0;
    }
    switch (type) {
        case CONNECTION_REQUEST :
            log(DEBUG, "send CONNECTION_REQUEST\n");
            sendLen = CONNECTION_REQUEST_LENGTH;
            sendBuffer = (char *)malloc(sendLen + 1);
            memset(sendBuffer, 0x00, sendLen + 1);
            writePos = sendBuffer;
            writePos[0] = type;
            writePos += CHAR_SIZE;
            htonlVal = htonl(requestMessageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            htonlVal = htonl(20);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            sprintf(writePos, "%s", connData->getSessionKey());
            //60초 이내에 응답이 없으면 종료하고 다시 연결. 
            Utils::startTimer(&this->keepAliveRespTimerId, this->keepAliveResponseTimerFunc, 
                                                           this, KEEP_ALIVE_RESPONSE_TIMER_MS_INTERVAL, 0);
            log(DEBUG, "this->keepAliveRespTimerId = %d\n", this->keepAliveRespTimerId);
            break;
        case PUSH_RESPONSE :
            log(DEBUG, "send PUSH_RESPONSE==========================================\n");
            sendLen = PUSH_MESSAGE_RESPONSE_LENGTH;
            sendBuffer = (char *)malloc(sendLen + 1);
            memset(sendBuffer, 0x00, sendLen + 1);
            writePos = sendBuffer;
            writePos[0] = type;
            log(DEBUG, "resp type = %c\n", type);
            writePos += CHAR_SIZE;
            htonlVal = htonl(messageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            log(DEBUG, "resp message id = %d\n", messageId);
            writePos += INT_SIZE;
            // body size - RT + correlator
            htonlVal = htonl(8);
            memcpy(writePos, &htonlVal, sizeof(int));
            log(DEBUG, "resp body size = %d\n", 8);
            writePos += INT_SIZE;
            memcpy(writePos, PUSH_IF_OK, strlen(PUSH_IF_OK));
            log(DEBUG, "resp RT = %s\n", PUSH_IF_OK);

            writePos += INT_SIZE;
            htonlVal = htonl(correlator);
            memcpy(writePos, &htonlVal, sizeof(int));
            log(DEBUG, "resp correlator = %d\n", correlator);
            log(DEBUG, "==========================================\n");
            break;

        case KEEP_ALIVE_REQUEST :
            log(DEBUG, "send KEEP_ALIVE_REQUEST\n");
            sendLen = KEEP_ALIVE_REQUEST_LENGTH;
            sendBuffer = (char *)malloc(sendLen + 1);
            memset(sendBuffer, 0x00, sendLen + 1);
            writePos = sendBuffer;
            writePos[0] = type;
            writePos += CHAR_SIZE;
            htonlVal = htonl(requestMessageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            // body size
            htonlVal = htonl(4);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            // write body. correlator == requestMessagId
            htonlVal = htonl(requestMessageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            //60초 이내에 응답이 없으면 종료하고 다시 연결. 
            Utils::startTimer(&this->keepAliveRespTimerId, this->keepAliveResponseTimerFunc, 
                                                             this, KEEP_ALIVE_RESPONSE_TIMER_MS_INTERVAL, 0);
            break;
        case KEEP_ALIVE_CHANGE_RESPONSE :
            log(DEBUG, "send KEEP_ALIVE_CHANGE_RESPONSE\n");
            sendLen = KEEP_ALIVE_REQUEST_LENGTH;
            sendBuffer = (char *)malloc(sendLen + 1);
            memset(sendBuffer, 0x00, sendLen + 1);
            writePos = sendBuffer;
            writePos[0] = type;
            writePos += CHAR_SIZE;
            htonlVal = htonl(messageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            // body size
            htonlVal = htonl(4);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            // write body. 
            memcpy(writePos, PUSH_IF_OK, strlen(PUSH_IF_OK));
            break;

        case APP_UPDATE_RESPONSE :
        case PUSH_AGENT_UPDATE_RESPONSE :
            log(DEBUG, "send PUSH_AGENT_UPDATE_RESPONSE or APP_UPDATE_RESPONSE\n");
            sendLen = APP_UPDATE_RESPONSE_LENGTH;
            sendBuffer = (char *)malloc(sendLen + 1);
            memset(sendBuffer, 0x00, sendLen + 1);
            writePos = sendBuffer;
            writePos[0] = type;
            writePos += CHAR_SIZE;
            htonlVal = htonl(messageId);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            // body size
            htonlVal = htonl(4);
            memcpy(writePos, &htonlVal, sizeof(int));
            writePos += INT_SIZE;
            memcpy(writePos, PUSH_IF_OK, strlen(PUSH_IF_OK));
            break;

            break;
            
        case PUSH_REQUEST:
        case KEEP_ALIVE_RESPONSE :
        case KEEP_ALIVE_CHANGE_REQUEST :
        case APP_UPDATE_REQUEST :
        case PUSH_AGENT_UPDATE_REQUEST :
        case CONNECTION_RESPONSE :
        default :
            log(ERROR, "ERROR Unknown or un-supported message type\n");
            return -1;
    }
    
    int numWrite = write(sockfd, sendBuffer, sendLen);
    if (numWrite < 0) {
        log(ERROR, "ERROR writing to socket\n");
        return -1;
    }
    
    return OK;
}

void PushConnection::handlePushMessageFromServer(PushMessageType type, int messageId, int bodyLen, char *body) {
    log(DEBUG, "handlePushMessageFromServer type = %c\n", type);
    char *pos = body;
    int retryConnection = 0;
    int retryWaitTime = RETRY_TIMER_MS_INTERVAL;
    
    if (bodyLen <= 0 || body == NULL) {
        if (body != NULL) {
            free(body);
        }
        return;
    }
    switch (type) {
        case CONNECTION_RESPONSE :
            log(DEBUG, "CONNECTION_RESPONSE received. send KEEP_ALIVE_REQUEST.\n");
            if (this->keepAliveRespTimerId > 0) {
                Utils::stopTimer(this->keepAliveRespTimerId);
                this->keepAliveRespTimerId = 0;
            }
            if (strncmp(PUSH_IF_OK, body, strlen(PUSH_IF_OK)) != 0) {
                retryConnection = 1;
                break;
            } 
            sendRequestOrResponseToServer(KEEP_ALIVE_REQUEST);
            break;
            
        case PUSH_REQUEST : {
            log(DEBUG, "PUSH_REQUEST received-----------------------------\n");
            int correlator = this->ntohlFromCharArray(pos);
            log(DEBUG, "corrleator = %d\n", correlator);
            pos += INT_SIZE;
            log(DEBUG, "message ID = %d\n", messageId);
            log(DEBUG, "app ID = %s\n", pos);
            // skip appID and repeat key
            pos += (APPID_SIZE);
            // skip repeat key
            log(DEBUG, "repeat key = %s\n", pos);
            pos += (REPEAT_KEY_SIZE);
            // skip alert 
            pos += ALERT_SIZE;
            log(DEBUG, "Payload data = %s\n", pos);
            log(DEBUG, "---------------------------------------------------\n");
            if (pos == NULL || strlen(pos) == 0) {
                log(ERROR, "Empty payload data !!!! Ignore this !!\n");
                break;
            }
            
            // TODO : malloc을 할 필요가 있는지 나중에 확인. 
            int payloadLen = strlen(pos);
            char *payload = (char*)malloc(payloadLen + 1);
            memset(payload, 0x00, payloadLen + 1);
            memcpy(payload, pos, payloadLen);
            log(DEBUG, "Copied payload data = %s\n", payload);
            
            Document resDoc;
            resDoc.Parse(payload);
            
            if (resDoc["SERVICE_TYPE"].IsString()) {
                char *serviceType = (char*)resDoc["SERVICE_TYPE"].GetString();
                /**
                #define SERVICE_TYPE_REALTIME_BROADCAST = "00";
                #define SERVICE_TYPE_NORMAL_BROADCAST = "01";
                #define SERVICE_TYPE_TEXT_BROADCAST = "02";
                #define SERVICE_TYPE_EMERGENCY_CALL = "03";
                #define SERVICE_TYPE_INHABITANTS_POLL  = "04";
                #define SERVICE_TYPE_COOPERATIVE_BUYING = "05";
                #define SERVICE_TYPE_IOT_DEVICE_CONTROL = "06";
                #define SERVICE_TYPE_PUSH_NOTIFICATION = "07";
                #define SERVICE_TYPE_FIND_PASSWORD = "08";
                */
                if (strcmp(SERVICE_TYPE_REALTIME_BROADCAST, serviceType) == 0 ||
                    strcmp(SERVICE_TYPE_NORMAL_BROADCAST, serviceType) == 0 /*|| 
                    strcmp(SERVICE_TYPE_TEXT_BROADCAST, serviceType) == 0*/) {
                    // send to media play
                    log(INFO, "Broadcast service type... %s\n", serviceType);
                } else {
                    log(INFO, "Not supported service type... %s\n", serviceType);
                }
            }
            sendRequestOrResponseToServer(PUSH_RESPONSE, messageId, correlator);
           
            break;
        }
            
        case KEEP_ALIVE_RESPONSE :
            if (this->keepAliveRespTimerId > 0) {
                Utils::stopTimer(this->keepAliveRespTimerId);
                this->keepAliveRespTimerId = 0;
            }
            log(DEBUG, "KEEP_ALIVE_RESPONSE received. result = %s\n", body);
            if (strncmp(PUSH_IF_OK, body, strlen(PUSH_IF_OK)) != 0) {
                log(DEBUG, "KEEP_ALIVE_RESPONSE is error code. retry.\n");
                retryWaitTime = SECONDS_TO_MILLISECONDS;
                retryConnection = 1;
                break;
            } 
            log(DEBUG, "KEEP_ALIVE_RESPONSE is surccess. start keep-alive period timer.\n");
            // set keep-alive timer
            Utils::startTimer(&this->keepAliveTimerId, keepAliveTimerFunc, this, this->getKeepAliveTimeSeconds() * SECONDS_TO_MILLISECONDS, 0);
            
            break;
        case KEEP_ALIVE_CHANGE_REQUEST : {
            int changedPeriod = atoi(body);
            if (changedPeriod >= 0) {
                this->connData->setKeepAlivePeriod(changedPeriod);
            }
            sendRequestOrResponseToServer(KEEP_ALIVE_CHANGE_RESPONSE, messageId, -1);
            break;
        }
        
        case PUSH_AGENT_UPDATE_REQUEST :{
            log(DEBUG, "PUSH_AGENT_UPDATE_REQUEST received-----------------------------\n");
            int correlator = this->ntohlFromCharArray(pos);
            log(DEBUG, "message ID = %d\n", messageId);
            // TODO : malloc을 할 필요가 있는지 나중에 확인. 
            int len = strlen(pos);
            char *downloadUrl = (char*)malloc(len + 1);
            memset(downloadUrl, 0x00, len + 1);
            memcpy(downloadUrl, pos, len);
            log(DEBUG, "Download URL = %s\n", downloadUrl);
            // skip download url
            pos += URL_SIZE;
            
            // TODO : malloc을 할 필요가 있는지 나중에 확인. 
            len = strlen(pos);
            char *reportUrl = (char*)malloc(len + 1);
            memset(reportUrl, 0x00, len + 1);
            memcpy(reportUrl, pos, len);
            log(DEBUG, "Report URL = %s\n", reportUrl);
            log(DEBUG, "---------------------------------------------------\n");
            
            sendRequestOrResponseToServer(PUSH_AGENT_UPDATE_RESPONSE, messageId, -1);
            break;
        }
        
        case APP_UPDATE_REQUEST : {
            log(DEBUG, "APP_UPDATE_REQUEST received-----------------------------\n");
            int correlator = this->ntohlFromCharArray(pos);
            log(DEBUG, "message ID = %d\n", messageId);
            // TODO : malloc을 할 필요가 있는지 나중에 확인. 
            int len = strlen(pos);
            char *appId = (char*)malloc(len + 1);
            memset(appId, 0x00, len + 1);
            memcpy(appId, pos, len);
            log(DEBUG, "appID = %s\n", appId);
            // skip download url
            pos += URL_SIZE;
            
            // TODO : malloc을 할 필요가 있는지 나중에 확인. 
            len = strlen(pos);
            char *downloadUrl = (char*)malloc(len + 1);
            memset(downloadUrl, 0x00, len + 1);
            memcpy(downloadUrl, pos, len);
            log(DEBUG, "Download URL = %s\n", downloadUrl);
            log(DEBUG, "---------------------------------------------------\n");
            
            sendRequestOrResponseToServer(APP_UPDATE_RESPONSE, messageId, -1);
            break;
        }
            
        case PUSH_RESPONSE :
        case KEEP_ALIVE_REQUEST :
        case APP_UPDATE_RESPONSE :
        case KEEP_ALIVE_CHANGE_RESPONSE :
        case CONNECTION_REQUEST :
        case PUSH_AGENT_UPDATE_RESPONSE :
        default :
            if (body != NULL) {
                free(body);
            }
            log(ERROR, "ERROR Unknown or un-supported message type\n");
            return;
    }
    if (body != NULL) {
        free(body);
    }
    
    if (retryConnection) {
        log(ERROR, "ERROR Server response error. Retry after %d milli-seconds.\n", retryWaitTime);
        // connection failed from server 
        // ?? what to do ???
        stopPushAgent();
        // retry after 5 min.
        Utils::startTimer(&this->retryConnectionTimerId, retryTimerFunc, this, retryWaitTime, 0);
    }
}

/**
 * Read thread.
 */
int PushConnection::runReadThread() {
    int res;  
  
    log(DEBUG, "run read thread...\n");
    res = pthread_attr_init(&attr);  
    if (res != 0) {  
        log(ERROR, "Attribute init failed");  
        return READ_THREAD_FAILED;
    }  
    res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);  
    if (res != 0) {  
        log(ERROR, "Setting detached state failed");  
        return READ_THREAD_FAILED;
    }  
  
    signal(SIGUSR1, readThreadSignalHandler);
    res = pthread_create(&readThread, &attr, ReadThreadFunc, this);  
    if (res != 0) {  
        log(ERROR, "Creation of thread failed");  
        return READ_THREAD_FAILED;
    }  
    
    return OK;
}

void PushConnection::readThreadSignalHandler(int signum) { pthread_exit(NULL); }

int PushConnection::ntohlFromCharArray(char* arr){
    if (arr == NULL) {
        return 0;
    }
    
    int i = 0; 
    int ret = 0;
    for (i = INT_SIZE; i > 0; i--) {
        ret += arr[i] << (i * 8);
    }
    return ntohl(ret);
}

PushMessageHeader PushConnection::parseMessageHeader(char* message) {
    PushMessageHeader msg;    
    msg.type = UNKNOWN_MESSAGE_TYPE;

    if (message == NULL) {
        return msg;
    }
    
    char *readPos = message;
    msg.type = (PushMessageType)readPos[0];
    if (msg.type < CONNECTION_REQUEST || msg.type > APP_UPDATE_RESPONSE) {
        msg.type = UNKNOWN_MESSAGE_TYPE;
        return msg;
    }
    log(DEBUG, "Read message type = %d\n", msg.type);
    readPos += CHAR_SIZE;
    msg.messageId = ntohlFromCharArray(readPos);
    log(DEBUG, "     message ID = %d\n", msg.messageId);
    readPos += INT_SIZE;
    msg.bodyLen = ntohlFromCharArray(readPos);
    log(DEBUG, "     bodyLen = %d\n", msg.bodyLen);
    
    return msg;
}

void* PushConnection::ReadThreadFunc(void* objParam) {  
    PushConnection *thr  = ((PushConnection *)objParam); 
    int sockfd = thr->getSockfd();
    char recvBodyBuffer[TCP_READ_BUFFER_SIZE];
    char recvHeaderBuffer[PUSH_MESSAGE_HEADER_LENGHT + 1];
    int recvBufferSize = 0;
    char *recvBuffer;
    
    PushMessageHeader messageHeader;
    char *messageBody = NULL;
    
    int numRead = 0;
    int isContinue = 0;
    
    static sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    
    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) != 0) {
        log(ERROR, "pthread_sigmask\n");
    }
    log(DEBUG, "pthread_id = %d, sockfd = %d\n", pthread_self(), sockfd);  
    while (1) {
        if (isContinue) {
            memset(recvBodyBuffer, 0x00, TCP_READ_BUFFER_SIZE);
            recvBufferSize = TCP_READ_BUFFER_SIZE - 1;
            recvBuffer = recvBodyBuffer;
        } else {
            memset(recvHeaderBuffer, 0x00, PUSH_MESSAGE_HEADER_LENGHT + 1);
            recvBufferSize = PUSH_MESSAGE_HEADER_LENGHT;
            recvBuffer = recvHeaderBuffer;
        } 
        if ((numRead = read(sockfd, recvBuffer, (size_t)recvBufferSize)) <= 0) {
            break;
        }
        log(DEBUG, "ReadThreadFunc %d recv numRead = %d\n", pthread_self(), numRead);
        char *readPos = recvBuffer;
        if (isContinue) {
            if (numRead > 0 && messageHeader.restChunkLen > 0) {
                if (numRead  == messageHeader.restChunkLen) {
                    log(DEBUG, "Copy messagebody(continue last) = %d\n", numRead);
                    memcpy(messageBody + (messageHeader.bodyLen - messageHeader.restChunkLen), (char*)recvBuffer, numRead);
                    log(DEBUG, "Copy messagebody(continue partial) = %s\n", messageBody);
                    isContinue = 0;
                    messageHeader.restChunkLen = 0;
                    thr->handlePushMessageFromServer(messageHeader.type, messageHeader.messageId, messageHeader.bodyLen, messageBody);
                } else {
                    log(DEBUG, "Copy messagebody(continue partial) len = %d\n", numRead);
                    memcpy(messageBody + (messageHeader.bodyLen - messageHeader.restChunkLen), (char*)recvBuffer, numRead);
                    log(DEBUG, "Copy messagebody(continue partial) = %s\n", messageBody);
                    messageHeader.restChunkLen -= numRead;
                    isContinue = 1;
                }
            }
        } else {
            messageHeader = thr->parseMessageHeader(recvBuffer);
            if (messageHeader.type != UNKNOWN_MESSAGE_TYPE) {
                if (numRead - PUSH_MESSAGE_HEADER_LENGHT == 0 && messageHeader.bodyLen > 0) {
                    // read only header.
                    messageBody = (char*)malloc(messageHeader.bodyLen + 1);
                    memset(messageBody, 0x00, messageHeader.bodyLen + 1);
                    isContinue = 1;
                    messageHeader.restChunkLen = messageHeader.bodyLen;
                } else if (numRead - PUSH_MESSAGE_HEADER_LENGHT > 0 && messageHeader.bodyLen > 0) {
                    messageBody = (char*)malloc(messageHeader.bodyLen + 1);
                    memset(messageBody, 0x00, messageHeader.bodyLen + 1);
                    if (numRead - PUSH_MESSAGE_HEADER_LENGHT == 0) {
                        // read only header.
                        isContinue = 1;
                        messageHeader.restChunkLen = messageHeader.bodyLen;
                    } else if (numRead - PUSH_MESSAGE_HEADER_LENGHT == messageHeader.bodyLen) {
                        log(DEBUG, "Copy messagebody(whole) = %d\n", messageHeader.bodyLen);
                        memcpy(messageBody, (char*)recvBuffer + PUSH_MESSAGE_HEADER_LENGHT, messageHeader.bodyLen);
                        isContinue = 0;
                        thr->handlePushMessageFromServer(messageHeader.type, messageHeader.messageId, messageHeader.bodyLen, messageBody);
                    } else {
                        log(DEBUG, "Copy messagebody(partial) len = %d\n", numRead - PUSH_MESSAGE_HEADER_LENGHT);
                        memcpy(messageBody, (char*)recvBuffer + PUSH_MESSAGE_HEADER_LENGHT, numRead - PUSH_MESSAGE_HEADER_LENGHT);
                        log(DEBUG, "Copy messagebody(partial) = %s\n", messageBody);
                        messageHeader.restChunkLen = messageHeader.bodyLen - (numRead - PUSH_MESSAGE_HEADER_LENGHT);
                        isContinue = 1;
                    }
                }
            }
        }
    }
    
    log(DEBUG, "Unblocking SIGUSR1 in thread...\n");
    if (pthread_sigmask(SIG_UNBLOCK, &mask, NULL) != 0) {
        log(ERROR, "pthread_sigmask\n");
    }
//    if (errno == EINTR) {
//        log(DEBUG, "ReadThreadFunc closed by internal\n");
//    }
    log(ERROR, "ReadThreadFunc recv numRead = %d\n", numRead);
    
    return NULL;
}  


// timer callback function 
void PushConnection::retryTimerFunc(int sig, siginfo_t *si, void *uc) {  
    if (si->si_value.sival_ptr == NULL) {
        log(ERROR, "sival_ptr is NULL\n");
        return;
    }
    PushConnection *thr  = (PushConnection *)(si->si_value.sival_ptr); 
    log(DEBUG, "retryTimerFunc called. connect to push server\n");
    if (thr->retryConnectionTimerId > 0) {
        Utils::stopTimer(thr->retryConnectionTimerId);
    }
    thr->retryConnectionTimerId = 0;
    thr->startPushAgent();
}  

void PushConnection::keepAliveTimerFunc(int sig, siginfo_t *si, void *uc) {
    if (si->si_value.sival_ptr == NULL) {
        log(ERROR, "sival_ptr is NULL\n");
        return;
    }
    log(DEBUG, "keepAliveTimerFunc called. send keep-alive message to push server\n");
    PushConnection *thr  = (PushConnection *)(si->si_value.sival_ptr); 
    if (thr->keepAliveTimerId > 0) {
        Utils::stopTimer(thr->keepAliveTimerId);
    }
    thr->keepAliveTimerId = 0;
    int ret = thr->sendRequestOrResponseToServer(KEEP_ALIVE_REQUEST);
    if (ret != OK) {
        thr->stopPushAgent();
        Utils::startTimer(&thr->retryConnectionTimerId, retryTimerFunc, thr, NOT_RESPOND_RETRY_TIMER_MS_INTERVAL, 0);
    } else {
        // set keep-alive timer
        Utils::startTimer(&thr->keepAliveTimerId, keepAliveTimerFunc, thr, thr->getKeepAliveTimeSeconds() * SECONDS_TO_MILLISECONDS, 0);
    }
}  

void PushConnection::keepAliveResponseTimerFunc(int sig, siginfo_t *si, void *uc) {  
    if (si->si_value.sival_ptr == NULL) {
        log(ERROR, "sival_ptr is NULL\n");
        return;
    }
    PushConnection *thr  = (PushConnection *)(si->si_value.sival_ptr); 
    if (thr->keepAliveRespTimerId > 0) {
        Utils::stopTimer(thr->keepAliveRespTimerId);
    }
    thr->keepAliveRespTimerId = 0;
    log(DEBUG, "keepAliveResponseTimerFunc called. not respond. stop push agent and reconnect\n");

    thr->stopPushAgent();
    Utils::startTimer(&thr->retryConnectionTimerId, retryTimerFunc, thr, NOT_RESPOND_RETRY_TIMER_MS_INTERVAL, 0);
}  

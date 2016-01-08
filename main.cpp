#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "SHA1.h"
#include "utils.h"
#include "messagequeue.h"
#include "constants.h"

#include "pushgateway.h"
#include "pushconnection.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"

#include <sys/time.h>

using namespace std;
using namespace rapidjson;

void timerHandler(int sig, siginfo_t *si, void *uc);
void messageQueueFunc(mqd_t *mqDes, void *buffer, int bufferSize, void *userData);

int readIfaceUrlFromFile(char **ifaceServerAddr) {
    FILE* fi;
    char tmp[MAX_IFACE_URL_SIZE];
    memset(tmp, 0, sizeof(tmp));
    
    if ((fi = fopen("./serverInfo", "r")) != NULL) {
        while (fgets(tmp, MAX_IFACE_URL_SIZE, fi) != NULL) {
            cout << "Read from file ifaceServerAddr = " << tmp << endl;
            
            *ifaceServerAddr = (char *)malloc(MAX_IFACE_URL_SIZE);
            if (!Utils::startsWith("http://", tmp)) {
                memcpy(*ifaceServerAddr, "http://", strlen("http://"));
                memcpy(*ifaceServerAddr + strlen("http://"), tmp, strlen(tmp));
            } else {
                memcpy(*ifaceServerAddr, tmp, strlen(tmp));
            }
            
            fclose(fi);
            return 1;
        }
    }
    
    if (fi != NULL) {
        fclose(fi);
    }
    return 0;
}

int writeIfaceUrlToFile(char *ifaceServerAddr) {
    FILE* fo;
    int ret = 0;
    
    if (ifaceServerAddr == NULL || strlen(ifaceServerAddr) == 0) {
        cerr << "ifaceServerAddr is null or 0-length" << endl;
        return 0;
    }
    
    if ((fo = fopen("./serverInfo", "w")) != NULL) {
        cout << "Write to file ifaceServerAddr = " << ifaceServerAddr << endl;
        if (!Utils::startsWith("http://", ifaceServerAddr)) {
            fprintf(fo, "%s%s%s", "http://", ifaceServerAddr, PUSH_IF_CONTEXT);
        } else {
            fprintf(fo, "%s%s", ifaceServerAddr, PUSH_IF_CONTEXT);
        }
        ret = 1;
    }
    
    fclose(fo);
    return ret;
}

char* readDeviceIdFromFile() {
    FILE* fi;
    char tmp[MAX_IFACE_URL_SIZE];
    memset(tmp, 0, sizeof(tmp));
    
    if ((fi = fopen("./deviceInfo", "r")) != NULL) {
        while (fgets(tmp, MAX_IFACE_URL_SIZE, fi) != NULL) {
            cout << "Read from file deviceId = " << tmp << endl;
            
            fclose(fi);
            return tmp;
        }
    }
    
    if (fi != NULL) {
        fclose(fi);
    }
    return tmp;
}

int writeDeviceIdToFile(char *deviceId) {
    FILE* fo;
    int ret = 0;
    
    if (deviceId == NULL || strlen(deviceId) == 0) {
        cerr << "deviceId is null or 0-length" << endl;
        return 0;
    }
    
    if ((fo = fopen("./deviceInfo", "w")) != NULL) {
        cout << "Write to file deviceId = " << deviceId << endl;
        fprintf(fo, "%s", deviceId);
        ret = 1;
    }
    
    fclose(fo);
    return ret;
}

#define DEVICE_ID_LENGTH                40

int main(int argc, char **argv) {
    PushGateway pushGateway;
    PushConnection *pushConnection;
    
    
    char *savedDeviceId = readDeviceIdFromFile();
    char deviceId[DEVICE_ID_LENGTH + 1]; // 40 chars + a zero
    deviceId[DEVICE_ID_LENGTH] = 0;

    if (savedDeviceId == NULL || strlen(savedDeviceId) == 0) {
        unsigned char macAddress[6];
        if (!Utils::getMacAddress(macAddress)) {
            log(ERROR, "getMacAddress() failed...");
            // check network status ????
        } else {
            for (int i = 0; i < 6; i++) {
                log(DEBUG, "%d value = %d\n", i, *(macAddress + i));
            }
            log(DEBUG, "%02x:%02x:%02x:%02x:%02x:%02x\n",
                    *(macAddress),
                    *(macAddress + 1),
                    *(macAddress + 2),
                    *(macAddress + 3),
                    *(macAddress + 4),
                    *(macAddress + 5)
                );
            
            // Unittest
            unsigned char hash[20];
            int end = 0;

            SHA1::calc(macAddress, 6, hash);
            SHA1::toHexString(hash, deviceId);
            log(INFO, "SHA1 hex string = %s\n", deviceId);
            memcpy(deviceId, "d69f0a84d4cc3d80261298ad1edf5cdfee94bfdc", DEVICE_ID_LENGTH);
            writeDeviceIdToFile(deviceId);
        }
        
    } else {
        memcpy(deviceId, savedDeviceId, DEVICE_ID_LENGTH);
    }
    
    // Read Interface Server URL from file 
    FILE* fi;
    char* ifaceServerAddr;
    
    // set message queue 
    MessageQueueInfo info;
    info.mqName = (char*)malloc(strlen(MQ_NAME_MAINQUEUE) + 1);
    memset(info.mqName, 0x00, strlen(MQ_NAME_MAINQUEUE) + 1);
    memcpy(info.mqName, MQ_NAME_MAINQUEUE, strlen(MQ_NAME_MAINQUEUE));
    info.mqNameSize = strlen(MQ_NAME_MAINQUEUE);
    info.flag = O_CREAT | O_RDWR | O_NONBLOCK;
    info.Func = messageQueueFunc;
    
    MessageQueue* mq = new MessageQueue();
    mq->notifySetup(info);
    
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MESSAGE_QUEUE_MSGSIZE;
    mqd_t mqDes = mq_open(MQ_NAME_MAINQUEUE, O_WRONLY|O_CREAT, 0655, &attr);
    if (mqDes == -1) {
        log(ERROR, "Send mq_open failed.\n");
        exit(0);
    }

    // connect to push agent
    if (readIfaceUrlFromFile(&ifaceServerAddr)) {
        // get push gateway information and start push agent 
        log(DEBUG, "Iface Server URL = %s\n", ifaceServerAddr);
        
        pushGateway.setIfaceServerAddr(ifaceServerAddr);
        http_retcode ret = (http_retcode)200;
        PushConnData* connData = pushGateway.getPushGatewayInfoFromServer(deviceId, &ret);
        if (ret != 200) {
            if (ret != ERRSERVER) {
            // retry after 1 minutes...
            } else {
                log(ERROR, "Server returns error !!!!\n");
            }
        } else {
            // connect to push gateway server
            log(DEBUG, "Connect to push gateway server\n");
            if (connData == NULL) {
            } else {
                /*char *deviceId = (char *)malloc(41);
                memset(deviceId, 0x00, 41);
                memcpy(deviceId, hexstring, 41);*/
                pushConnection = new PushConnection(/*deviceId*/);
                pushConnection->setPushConnectionData(connData);
                pushConnection->startPushAgent();
                
    // timer test
    //timer_t timerId;
    //Utils::startTimer(&timerId, timerHandler, pushConnection, 60000, 1);
                
            }
        }
    } else {
        // do nothing...
        log(DEBUG, "can't read push interface server info.!!!");
        sleep(3);
    }
    
    while (1) {
        sleep(5);
        pushConnection->stopPushAgent();
        /*
        cout << "send message" << endl;

        char message[100];
        memset(message, 0x00, 100);
        message[0] = MQ_CMD_IFACE_URL_CHANGED;
        strncpy(message + 1, "ddd", strlen("ddd"));
        
        int status = mq_send(mqDes, message, strlen(message) + 1, 0);
        if (status == -1) {
            perror("mq_send failure"), 
            exit(0);
        }
        */
    }
    
    return 0;
}

void timerHandler(int sig, siginfo_t *si, void *uc) {
    //timer_t tidp  = si->si_value.sival_ptr;
   PushConnection *thr  = (PushConnection *)(si->si_value.sival_ptr); 
   thr->stopPushAgent();
    // begin
    struct timeval tv, tvEnd, tvDiff;
    gettimeofday(&tv, NULL);
    char buffer[30];
    time_t curtime;

    log(DEBUG, "%ld.%06ld", tv.tv_sec, tv.tv_usec);
    curtime = tv.tv_sec;
    strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
    log(DEBUG, " = %s.%06ld\n", buffer, tv.tv_usec);
}

void messageQueueFunc(mqd_t *mqDes, void *buffer, int bufferSize, void* userData) {
    log(DEBUG, "message received, mqDes = %d\n", *mqDes);
    log(DEBUG, "        size = %d, buffer = %s\n", bufferSize, (char *)buffer);
    
    char *msg = (char*)buffer;
    if (msg == NULL || strlen(msg) == 0) {
        return;
    }
    
    char msgId = (char)msg[0];
    char *msgBuf = (char*)(msg + 1);
    
    // Constants.h 참조 .
    switch (msgId) {
        case MQ_CMD_IFACE_URL_CHANGED:
           log(DEBUG, "MQ_CMD_IFACE_URL_CHANGED received\n");
           break;
           
        case MQ_CMD_KEEPALIVE_TIMER_EXPIRED:
           log(DEBUG, "MQ_CMD_KEEPALIVE_TIMER_EXPIRED received\n");
           break;

        case MQ_CMD_PUSHAGENT_CONN_TERMINATED:
           log(DEBUG, "MQ_CMD_PUSHAGENT_CONN_TERMINATED received\n");
           break;

        default :
            log(DEBUG, "Unknown message ID received.\n");
            break;
    }
    if (bufferSize > 0) {
        free(buffer);
    }
}


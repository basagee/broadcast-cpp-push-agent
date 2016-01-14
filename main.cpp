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

#include "httplib.h"

#include <sys/time.h>
#include <net/if.h>

using namespace std;
using namespace rapidjson;

#define DEVICE_ID_LENGTH                        40
#define CHECK_NETWORK_STATUS_TIMER_INTERVAL     300000
#define RETRY_PUSH_GATEWAY_TIMER_INTERVAL       60000

void checkNetworkStatusTimerHandler(int sig, siginfo_t *si, void *uc);
void checkNetworkStatusTimerHandler2(int sig, siginfo_t *si, void *uc);
void checkNetworkStatusTimerHandler3(int sig, siginfo_t *si, void *uc);
void retryPushGatewayTimerHandler(int sig, siginfo_t *si, void *uc);
void messageQueueFunc(mqd_t *mqDes, void *buffer, int bufferSize, void *userData);
int readIfaceUrlFromFile(char **ifaceServerAddr);
int writeIfaceUrlToFile(char *ifaceServerAddr);
char* readDeviceIdFromFile();
int writeDeviceIdToFile(char *deviceId);
int getInfoAndStartPushAgent();

// .h
class PushAgentSingleton {
    public:
        static PushAgentSingleton& getInstance() {
            if (destroyed) {
                new(pInst) PushAgentSingleton; // 2)
                atexit(killPushAgentSingleton);
                destroyed = false;
            } else if (pInst == 0) {
                create();
            }
            return *pInst;
        }
        
        void setPushInterfaceServerAddress(char *ifaceAddr) { 
            if (this->pushInterfaceServerAddr != NULL) {
                log(DEBUG, "free pushInterfaceServerAddr\n");
                free(this->pushInterfaceServerAddr);
            }

            this->pushInterfaceServerAddr = NULL;
            if (ifaceAddr != NULL && strlen(ifaceAddr) > 0) {
                this->pushInterfaceServerAddr = (char*)malloc(strlen(ifaceAddr) + 1);
                memset(this->pushInterfaceServerAddr, 0x00, strlen(ifaceAddr) + 1);
                memcpy(this->pushInterfaceServerAddr, ifaceAddr, strlen(ifaceAddr)); 
            }
        }
        const char* getPushInterfaceServerAddress() { return this->pushInterfaceServerAddr; }
        void setDeviceId(char *deviceId) { 
            if (this->deviceId != NULL) {
                log(DEBUG, "free deviceId\n");
                free(this->deviceId);
            }

            this->deviceId = NULL;
            if (deviceId != NULL && strlen(deviceId) > 0) {
                this->deviceId = (char*)malloc(strlen(deviceId) + 1);
                memset(this->deviceId, 0x00, strlen(deviceId) + 1);
                memcpy(this->deviceId, deviceId, strlen(deviceId)); 
            }
        }
        const char* getDeviceId() { return this->deviceId; }
        PushConnection* getPushConnection() { return this->pushConnection; }
        void removeAllData() {
            log(DEBUG, "pInst->getPushInterfaceServerAddress() = %s\n", getPushInterfaceServerAddress());
            if (getPushInterfaceServerAddress() != NULL) {
                log(DEBUG, "pInst->setPushInterfaceServerAddress(NULL)\n");
                setPushInterfaceServerAddress(NULL);
            }
            if (getDeviceId() != NULL) {
                log(DEBUG, "pInst->setDeviceId(NULL)\n");
                setDeviceId(NULL);
            }
            if (getPushConnection() != NULL) {
                createNewPushConnection(1);
            }
        }
        
        timer_t retryPushGatewayTimerId;
        int processingRetryConnection;

        static void killPushAgentSingleton() {
            log(DEBUG, "killPushAgentSingleton()\n");
            static PushAgentSingleton inst;
            pInst = &inst;
            pInst->~PushAgentSingleton();  // 3)
        }
    private:
        PushAgentSingleton() {}
        PushAgentSingleton(const PushAgentSingleton & other);
        ~PushAgentSingleton() {
            destroyed = true;  // 1)
        }

        static void create() {
            static PushAgentSingleton inst;
            pInst = &inst;
            
            pInst->retryPushGatewayTimerId = 0;
            pInst->processingRetryConnection = 0;
            if (pInst->getPushConnection() == NULL) {
                log(DEBUG, "PushConnection created!!!!\n");
                pInst->createNewPushConnection(0);
            }
        }

        void createNewPushConnection(int justDelete) { 
            if (this->pushConnection != NULL) {
                this->pushConnection->stopPushAgent();
                log(DEBUG, "delete pushConnection\n");
                delete this->pushConnection;
            }
            if (!justDelete) {
                log(DEBUG, "create new PushConnection()\n");
                pushConnection = new PushConnection(); 
            }
        }

        static bool destroyed;
        static PushAgentSingleton* pInst;

    protected:
        char* pushInterfaceServerAddr;
        char* deviceId;
        PushConnection *pushConnection;
};

// .cpp 
bool PushAgentSingleton::destroyed = false;
PushAgentSingleton* PushAgentSingleton::pInst = 0;

int readIfaceUrlFromFile(char **ifaceServerAddr) {
    FILE* fi;
    char tmp[MAX_IFACE_URL_SIZE];
    memset(tmp, 0, sizeof(tmp));
    
    *ifaceServerAddr = NULL;
    if ((fi = fopen("./serverInfo", "r")) != NULL) {
        if (fgets(tmp, MAX_IFACE_URL_SIZE, fi) != NULL) {
            cout << "Read from fi le ifaceServerAddr = " << tmp << endl;
            
            if (strlen(tmp) > 0) {
                *ifaceServerAddr = (char *)malloc(MAX_IFACE_URL_SIZE);
                if (!Utils::startsWith("http://", tmp)) {
                    memcpy(*ifaceServerAddr, "http://", strlen("http://"));
                    memcpy(*ifaceServerAddr + strlen("http://"), tmp, strlen(tmp));
                } else {
                    memcpy(*ifaceServerAddr, tmp, strlen(tmp));
                }
            }
            
            fclose(fi);
            return 1;
        }
    }
    
    if (fi != NULL) {
        fclose(fi);
    }
    *ifaceServerAddr = NULL;
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

int getInfoAndStartPushAgent() {
    PushAgentSingleton::getInstance().processingRetryConnection = 0;
    PushGateway pushGateway;
    
    if (PushAgentSingleton::getInstance().getPushInterfaceServerAddress() == NULL) {
        log(ERROR, "PushInterfaceServerAddress is NULL. Do not anything...\n");
    }
    
    // get push gateway information and start push agent 
    log(DEBUG, "Iface Server URL = %s\n", PushAgentSingleton::getInstance().getPushInterfaceServerAddress());
    
    pushGateway.setIfaceServerAddr(PushAgentSingleton::getInstance().getPushInterfaceServerAddress());
    http_retcode ret = (http_retcode)200;
    PushConnData* connData = pushGateway.getPushGatewayInfoFromServer(PushAgentSingleton::getInstance().getDeviceId(), &ret);
    if (ret != 200) {
        // retry after 1 minutes...
        log(ERROR, "PushGateway server failed. Retry after 1 minutes;\n");
        Utils::startTimer(&PushAgentSingleton::getInstance().retryPushGatewayTimerId, retryPushGatewayTimerHandler, 
                                        NULL, RETRY_PUSH_GATEWAY_TIMER_INTERVAL, 0);
    } else {
        // connect to push gateway server
        log(DEBUG, "Connect to push gateway server = %s\n", connData->getGwIpAddr());
        if (connData == NULL) {
            PushAgentSingleton::getInstance().getPushConnection()->setPushConnectionData(NULL);
        } else {
            /*char *deviceId = (char *)malloc(41);
            memset(deviceId, 0x00, 41);
            memcpy(deviceId, hexstring, 41);*/
            PushAgentSingleton::getInstance().getPushConnection()->setPushConnectionData(connData);
            PushAgentSingleton::getInstance().getPushConnection()->startPushAgent();
        }
    }
    PushAgentSingleton::getInstance().processingRetryConnection = 0;
}

void setDeviceId() {
    if (PushAgentSingleton::getInstance().getDeviceId() != NULL) {
    }
    char *savedDeviceId = NULL;
    savedDeviceId = readDeviceIdFromFile();
    
    char *deviceId = (char*)malloc(DEVICE_ID_LENGTH + 1); // 40 chars + a zero
    memset(deviceId, 0x00, DEVICE_ID_LENGTH + 1);
    //deviceId[DEVICE_ID_LENGTH] = 0;

    if (savedDeviceId == NULL || strlen(savedDeviceId) == 0) {
        unsigned char macAddress[6];
        char ifaceName[IFNAMSIZ];
        if (!Utils::getMacAddress(macAddress, ifaceName)) {
            log(ERROR, "getMacAddress() failed...");
            // check network status ????
            free(deviceId);
            PushAgentSingleton::getInstance().setDeviceId(NULL);
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
            
            PushAgentSingleton::getInstance().setDeviceId(deviceId);
        }
    } else {
        memcpy(deviceId, savedDeviceId, DEVICE_ID_LENGTH);
        PushAgentSingleton::getInstance().setDeviceId(deviceId);
    }
    
    if (deviceId != NULL) {
        free(deviceId);
    }
}

int main(int argc, char **argv) {
    
    // first, message queue enable
    MessageQueueInfo info;
    memset(info.mqName, 0x00, MQ_NAME_MAINQUEUE_SIZE + 1);
    memcpy(info.mqName, MQ_NAME_MAINQUEUE, MQ_NAME_MAINQUEUE_SIZE);
    info.mqNameSize = MQ_NAME_MAINQUEUE_SIZE;
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

    // second, get and set deviceId
    setDeviceId();
    
    // third, Read Interface Server URL from file 
    // check network status
    if (!Utils::checkNetworkStatus()) {
        log(ERROR, "network unavailable...\n");
    }
    
    char* ifaceServerAddr = NULL;;
    readIfaceUrlFromFile(&ifaceServerAddr);
    PushAgentSingleton::getInstance().setPushInterfaceServerAddress(ifaceServerAddr);
    if (ifaceServerAddr != NULL) {
        free(ifaceServerAddr);
        ifaceServerAddr = NULL;
    }
    
    // fourth, check network status every 1minutes.
    timer_t checkNetworkStatusTimerId;
    Utils::startTimer(&checkNetworkStatusTimerId, checkNetworkStatusTimerHandler, 
                                    NULL, CHECK_NETWORK_STATUS_TIMER_INTERVAL, 1);
    
    // fifth, if (deviceId != NULL && ifaceServerAddr != NULL)
    //        then start push agent. 
    getInfoAndStartPushAgent();
    
    int i = 0;
    char c;
    while (c = fgetc(stdin)) {
        if (c == 'q') {
            break;
        }
    }
//    while (1) {
//        sleep(60);
        //log(DEBUG, "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
        //if (i % 2 == 0) PushAgentSingleton::getInstance().getPushConnection()->stopPushAgent();
        //else PushAgentSingleton::getInstance().getPushConnection()->startPushAgent();
        //i++;
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
//    }
    
    if (checkNetworkStatusTimerId > 0) {
        Utils::stopTimer(checkNetworkStatusTimerId);
    }
    
    if (mqDes > 0) {
        mq_close(mqDes);
        mq_unlink(MQ_NAME_MAINQUEUE);
    }
    if (mq != NULL) {
        delete mq;
    }
    
    PushAgentSingleton::getInstance().removeAllData();
    
    if (http_server != NULL) {
        free(http_server);
        http_server = NULL;
    }
    if (http_proxy_server != NULL) {
        free(http_proxy_server);
        http_proxy_server = NULL;
    }

    sleep(3);
    log(DEBUG, "exit(EXIT_SUCCESS)\n");
    exit(EXIT_SUCCESS);
}

void checkNetworkStatusTimerHandler(int sig, siginfo_t *si, void *uc) {
    //timer_t tidp  = si->si_value.sival_ptr;
    PushConnection *pushConnection  = PushAgentSingleton::getInstance().getPushConnection();
    
    if (Utils::checkNetworkStatus()) {
        log(DEBUG, "### Utils::checkNetworkStatus() is success!!!\n");
        if (!pushConnection->isAlive()) {
            if (PushAgentSingleton::getInstance().processingRetryConnection) {
                log(DEBUG, "### Previous retry connection is not completion. processing.....\n");
            } else {
                log(DEBUG, "### Network status is available...start push agent connection\n");
                getInfoAndStartPushAgent();
            }
        }
    } else {
        log(DEBUG, "### Network status is unavailable...stop push agent connection\n");
        if (pushConnection->isAlive()) {
            pushConnection->stopPushAgent();
        }
    }
    // begin
    /*
    struct timeval tv, tvEnd, tvDiff;
    gettimeofday(&tv, NULL);
    char buffer[30];
    time_t curtime;

    curtime = tv.tv_sec;
    strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
    log(DEBUG, "check time = %s.%06ld\n", buffer, tv.tv_usec);
    */
}

void retryPushGatewayTimerHandler(int sig, siginfo_t *si, void *uc) {
    //timer_t tidp  = si->si_value.sival_ptr;
    log(DEBUG, "### retryPushGatewayTimerHandler expired!!!!\n");
    
    if (PushAgentSingleton::getInstance().retryPushGatewayTimerId > 0) {
        Utils::stopTimer(PushAgentSingleton::getInstance().retryPushGatewayTimerId);
    }
    PushAgentSingleton::getInstance().retryPushGatewayTimerId = 0;

    getInfoAndStartPushAgent();
    // begin
    /*
    struct timeval tv, tvEnd, tvDiff;
    gettimeofday(&tv, NULL);
    char buffer[30];
    time_t curtime;

    curtime = tv.tv_sec;
    strftime(buffer, 30, "%m-%d-%Y  %T", localtime(&curtime));
    log(DEBUG, " = %s.%06ld\n", buffer, tv.tv_usec);
    */
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


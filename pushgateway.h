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
#ifndef _PUSHGATEWAY_H_
#define _PUSHGATEWAY_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "httplib.h"
}

class PushConnData {
    public:
        PushConnData() {
            this->sessionKey = NULL;
            this->authKey = NULL;
            this->gwIpAddr = NULL;
            this->gwPort = 0;
            this->keepAlivePeriod = 0;
        };
        PushConnData(char *sessionKey, char *authKey, char *gwIpAddr, int gwPort) {
            this->sessionKey = NULL;
            this->authKey = NULL;
            this->gwIpAddr = NULL;
            this->gwPort = 0;
            this->keepAlivePeriod = 0;
            
            setSessionKey(sessionKey);
            setAuthKey(authKey);
            setGwIpAddr(gwIpAddr);
            setGwPort(gwPort);
        }
        
        void setSessionKey(char *sessionKey) { 
            if (this->sessionKey != NULL) {
                free(this->sessionKey);
            }
            this->sessionKey = NULL;
            
            this->sessionKey = (char*)malloc(strlen(sessionKey) + 1);
            memset(this->sessionKey, 0x00, strlen(sessionKey) + 1);
            memcpy(this->sessionKey, sessionKey, strlen(sessionKey)); 
        }
        const char *getSessionKey(void) { 
            return this->sessionKey; 
            
        }
        
        void setAuthKey(char *authKey) { 
            if (this->authKey != NULL) {
                free(this->authKey);
            }
            this->authKey = NULL;
            
            this->authKey = (char*)malloc(strlen(authKey) + 1);
            memset(this->authKey, 0x00, strlen(authKey) + 1);
            memcpy(this->authKey, authKey, strlen(authKey)); 
        }
        const char *getAuthKey(void) { return this->authKey; }

        void setGwIpAddr(char *ipaddr) { 
            if (this->gwIpAddr != NULL) {
                free(this->gwIpAddr);
            }
            this->gwIpAddr = NULL;
            
            this->gwIpAddr = (char*)malloc(strlen(ipaddr) + 1);
            memset(this->gwIpAddr, 0x00, strlen(ipaddr) + 1);
            memcpy(this->gwIpAddr, ipaddr, strlen(ipaddr)); 
        }
        const char *getGwIpAddr(void) { return this->gwIpAddr; }

        void setGwPort(int gwPort) { this->gwPort = gwPort; }
        void setGwPort(char *gwPort) { 
            if (gwPort != NULL && strlen(gwPort) > 0) 
                this->gwPort = atoi(gwPort); 
            else 
                this->gwPort = 0; 
        }
        int getGwPort(void) { return this->gwPort; }
        
        void setKeepAlivePeriod(int keepAlivePeriod) { 
            this->keepAlivePeriod = keepAlivePeriod; 
            
        }
        void setKeepAlivePeriod(char *keepAlivePeriod) { 
            if (keepAlivePeriod != NULL && strlen(keepAlivePeriod) > 0) 
                this->keepAlivePeriod = atoi(keepAlivePeriod); 
            else 
                this->keepAlivePeriod = 0; 
        }
        int getKeepAlivePeriod(void) { return this->keepAlivePeriod; }
        
        ~PushConnData() {
            if (this->sessionKey != NULL) {
                free(sessionKey);
            }
            if (this->authKey != NULL) {
                free(authKey);
            }
            if (this->gwIpAddr != NULL) {
                free(gwIpAddr);
            }
        }
        
    private:
        char *sessionKey;
        char *authKey;
        char *gwIpAddr;
        int  gwPort;
        int  keepAlivePeriod;
};

class PushGateway {
public:
    PushGateway();
    PushGateway(const char* addr);
    ~PushGateway();
    
    bool setIfaceServerAddr(const char* addr);
    PushConnData* getPushGatewayInfoFromServer(const char* deviceId, http_retcode *retCode);

private:
    char* proxy = NULL;
    char* ifaceServerAddr = NULL;
    char *fileName = NULL;
    
    bool setProxy();
};

#endif                  // _PUSHGATEWAY_H_

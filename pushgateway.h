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
        PushConnData() {};
        PushConnData(char *sessionKey, char *authKey, char *gwIpAddr, int gwPort) {
            setSessionKey(sessionKey);
            setAuthKey(authKey);
            setGwIpAddr(gwIpAddr);
            setGwPort(gwPort);
        }
        
        void setSessionKey(char *sessionKey) { this->sessionKey = sessionKey; }
        const char *getSessionKey(void) { return this->sessionKey; }
        
        void setAuthKey(char *authKey) { this->authKey = authKey; }
        const char *getAuthKey(void) { return this->authKey; }

        void setGwIpAddr(char *gwIpAddr) { this->gwIpAddr = gwIpAddr; }
        const char *getGwIpAddr(void) { return this->gwIpAddr; }

        void setGwPort(int gwPort) { this->gwPort = gwPort; }
        void setGwPort(char *gwPort) { 
            if (gwPort != NULL && strlen(gwPort) > 0) 
                this->gwPort = atoi(gwPort); 
            else 
                this->gwPort = 0; 
        }
        int getGwPort(void) { return this->gwPort; }
        
        void setKeepAlivePeriod(int keepAlivePeriod) { this->keepAlivePeriod = keepAlivePeriod; }
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

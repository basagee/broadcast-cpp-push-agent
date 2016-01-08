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
#include <iostream>

#include "constants.h"
#include "pushgateway.h"
extern "C" {
#include "httplib.h"
}

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"

using namespace std;
using namespace rapidjson;

PushGateway::PushGateway() {
    setProxy();
}
PushGateway::PushGateway(const char* addr) {
    if (addr != NULL && strlen(addr) > 0) {
        ifaceServerAddr = (char*)malloc(strlen(addr) + 1);
        memset(ifaceServerAddr, 0x00, strlen(addr) + 1);
        memcpy(ifaceServerAddr, addr, strlen(addr));
    } else {
        ifaceServerAddr = NULL;
    }
    setProxy();
}

PushGateway::~PushGateway() {
    if (proxy != NULL) {
        free(http_proxy_server);
        http_proxy_server = NULL;
        http_proxy_port = 0;
    }
    
    if (ifaceServerAddr != NULL) {
        free(ifaceServerAddr);
        ifaceServerAddr = NULL;
    }
    
    if (fileName != NULL) {
        free(fileName);
        fileName = NULL;
    }
}

bool PushGateway::setIfaceServerAddr(const char* addr) {
    if (addr != NULL && strlen(addr) > 0) {
        ifaceServerAddr = (char*)malloc(strlen(addr) + 1);
        memset(ifaceServerAddr, 0x00, strlen(addr) + 1);
        memcpy(ifaceServerAddr, addr, strlen(addr));
    } else {
        ifaceServerAddr = NULL;
    }
    
    return true;
}

bool PushGateway::setProxy(void) {
    if ((proxy = getenv("http_proxy"))) {
        http_retcode ret = http_parse_url(proxy, &fileName);
        if (ret < 0) return false;
        http_proxy_server = http_server;
        http_server = NULL;
        http_proxy_port = http_port;
    }
    
    return true;
}

PushConnData* PushGateway::getPushGatewayInfoFromServer(const char* deviceId, http_retcode *retCode) {
    char typebuf[] = "application/json";
    int contentLength = 0;
    char *recvData = NULL;
    char *content = NULL;
    int recvLength = 0;
    
    if (deviceId == NULL || strlen(deviceId) <= 0) {
        log(ERROR, "Empty deviceID\n");
        *retCode = ERRUNKNOWN;
        return NULL;
    }
    
    http_retcode ret = http_parse_url(ifaceServerAddr, &fileName);
    if (ret < 0) {
        log(ERROR, "par URL error = %d\n", ret);
        *retCode = ret;
        return NULL;
    }

    Document reqDoc;
    reqDoc.SetObject();
    Document::AllocatorType& allocator = reqDoc.GetAllocator();
    
    Value object;
    
    object.SetString(deviceId, strlen(deviceId));
    reqDoc.AddMember("DEVICE_ID", object, allocator);
    object.SetString("IoT Gateway");
    reqDoc.AddMember("DEVICE_TYPE", object, allocator);
    object.SetString(VERSION);
    reqDoc.AddMember("VERSION", VERSION, allocator);
    object.SetString("NBPLUS");
    reqDoc.AddMember("MAKER", object, allocator);
    object.SetString("IoTGW");
    reqDoc.AddMember("MODEL", object, allocator);
    object.SetString("linux");
    reqDoc.AddMember("OS", object, allocator);

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    reqDoc.Accept(writer);

    content = (char*)buffer.GetString();
    if (content != NULL) {
        contentLength = strlen(content);
    }
    
    ret = http_post_keepopen(fileName, content, contentLength, 0, typebuf, &recvData, &recvLength);
    log(DEBUG, "ret = %d, recvType = %s, recvLength = %d\n", ret, typebuf, recvLength);
    log(DEBUG, "     recvContent = %s\n", recvData);
    
    if (ret == 200) {
        Document resDoc;
        resDoc.Parse(recvData);
        
        if (resDoc["RT"].IsString() && strcmp("0000", resDoc["RT"].GetString()) == 0) {
            PushConnData* connData = new PushConnData();
            connData->setSessionKey((char*)resDoc["SESSION_KEY"].GetString());
            connData->setAuthKey((char*)resDoc["DEVICE_AUTH_KEY"].GetString());
            connData->setGwIpAddr((char*)resDoc["CONN_IP"].GetString());
            connData->setGwPort((char*)resDoc["CONN_PORT"].GetString());
            connData->setKeepAlivePeriod((char*)resDoc["KEEP_ALIVE_PERIOD"].GetString());
            *retCode = ret;
            return connData;
        }
        *retCode = ERRSERVER;
        return NULL;
    }
        
    *retCode = ret;
    return NULL;
}




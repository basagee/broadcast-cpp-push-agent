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
#include "constants.h"
#include "messagequeue.h"

using namespace std;

#define PMODE 0655

/*
 * 메시지 큐를 열고 비동기로 notify setup을  설정한다. 
 */
mqd_t MessageQueue::notifySetup(MessageQueueInfo& info) {
    if (info.mqName == NULL || info.mqNameSize <= 0) {
        log(ERROR, "MessageQueueInfo.mqName is null\n");
        return -1;
    }
    
    if (info.flag & O_RDONLY == 0x00 && info.flag & O_WRONLY == 0x00 &&
        info.flag & O_RDWR == 0x00 && info.flag & O_NONBLOCK == 0x00 &&
        info.flag & O_CREAT == 0x00 && info.flag & O_EXCL == 0x00) {
        log(ERROR, "MessageQueueInfo.flag is invalid value = 0x%x", info.flag);
        return -1;
    }
    
    if (info.Func == NULL) {
        log(ERROR, "MessageQueueInfo.Func is null\n");
        return -1;
    }
    
    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_MESSAGE_QUEUE_MSGSIZE;
    
    // open message queue 
    mq_unlink(info.mqName);
    mqDes = mq_open(info.mqName, info.flag, PMODE, &attr);
    if (mqDes == (mqd_t) -1) {
        log(ERROR, "mq_open() failed...\n");
        return -1;
    }
    log(DEBUG, "Opened mqDes = %d\n", mqDes);

    // setup notify 
    if (notifySetup(&mqDes, info.Func, info.userData) == -1) {
        mq_close(mqDes);
        log(ERROR, "mq_notify() failed...\n");
        return -1;
    }
    
    return mqDes;
}

/*
 * 메시지 큐를 닫는다. 
 */
int MessageQueue::closeMessageQueue(mqd_t mqDes) {
    if (mqDes != -1) {
        return mq_close(mqDes);
    }
    return -1;
}

// private method
int MessageQueue::notifySetup(mqd_t* mqDes, void (*Func)(mqd_t* mqDes, void* buffer, int bufferSize, void* userData), void *userData) {
    struct sigevent sev;
    
    sev.sigev_notify = SIGEV_THREAD;            // notify via Thread
    sev.sigev_notify_function = &threadFunction;
    // Could be pointer to pthread_attr_t structure
    sev.sigev_notify_attributes = NULL;
    
    NotifiedMessageInfo* info = new NotifiedMessageInfo();
    info->Func = Func;
    info->mqDes = *mqDes;
    info->userData = userData;
    sev.sigev_value.sival_ptr = info;                 // Argument to thread function
    
    return mq_notify(*mqDes, &sev);
}

void MessageQueue::threadFunction(sigval_t sv) {
    ssize_t numRead = 0;
    mqd_t mqDes = -1;
    void *buffer = NULL;
    struct mq_attr attr;
    void (*Func)(mqd_t* mqDes, void* buffer, int bufferSize, void* userData);
    void* userData;
    
    NotifiedMessageInfo* info = (NotifiedMessageInfo*)sv.sival_ptr;
    mqDes = info->mqDes;
    Func = info->Func;
    userData = info->userData;
    delete info;
    
    if (mqDes == -1) {
        pthread_exit(NULL);
        return;
    }
    log(DEBUG, "threadFunction mqDes = %d\n", mqDes);
    
    notifySetup(&mqDes, Func, userData);
    if (mq_getattr(mqDes, &attr) == -1) {
        log(ERROR, "Can't get mq_getattr()\n");
        pthread_exit(NULL);
        return;
    }
    
    buffer = malloc(attr.mq_msgsize);
    if (buffer == NULL) {
        log(ERROR, "Can't malloc attr.mq_msgsize..\n");
        pthread_exit(NULL);
        return;
    }
    
    while ((numRead = mq_receive(mqDes, (char*)buffer, attr.mq_msgsize, NULL)) > 0) {
        // call callback funciton
        Func(&mqDes, buffer, attr.mq_msgsize, userData);
    }
    /*
    cout << "numRead value = " << numRead << endl;
    if (numRead != EAGAIN) {            // unexpected error..
        cerr << "mq_receive unexpected error = " << numRead << endl;
    }
    */
    pthread_exit(NULL);
    return;
}


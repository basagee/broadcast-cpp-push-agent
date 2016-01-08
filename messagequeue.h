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
#ifndef _MESSAGE_QUEUE_H_
#define _MESSAGE_QUEUE_H_

#include <iostream>
#include <pthread.h>
#include <mqueue.h>
#include <fcntl.h>              /* For definition of O_NONBLOCK */

#define MAX_MESSAGE_QUEUE_MSGSIZE 1024
/**
 * POSIX Message Queue를 참조하면 된다. 
 * http://www.joinc.co.kr/modules/moniwiki/wiki.php/man/3/mq_open
 */
class MessageQueueInfo {
    public :
        char *mqName;
        int mqNameSize;
        int flag;
        void (*Func)(mqd_t* mqDes, void* buffer, int bufferSize, void* userData);
        void *userData;
};

class NotifiedMessageInfo {
    public:
        mqd_t mqDes;
        void (*Func)(mqd_t* mqDes, void* buffer, int bufferSize, void* userData);
        void *userData;
};

class MessageQueue {
    public:
        /*
         * 메시지 큐를 열고 비동기로 notify setup을  설정한다. 
         */
        mqd_t notifySetup(MessageQueueInfo& info);
        /*
         * 메시지 큐를 닫는다. 
         */
        int closeMessageQueue(mqd_t mqDes);
        mqd_t getMessgeQueueDes();

    private:
        static int notifySetup(mqd_t* mqDes, void (*Func)(mqd_t* mqDes, void* buffer, int bufferSize, void* userData), void *userData);
        static void threadFunction(sigval_t sv);
        
        // member variables
        mqd_t mqDes;
        
};

#endif

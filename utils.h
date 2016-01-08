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
#ifndef _UTILS_H_
#define _UTILS_H_

#include <time.h>
#include <signal.h>
#include <unistd.h>

#define MAX_IFACE_URL_SIZE 256

class Utils {
    public:
        /*
         *  Get MAC Address 6 bytes array 
         */
        static int getMacAddress(unsigned char *getMacAddress, char *ifaceName);
        static int checkNetworkStatus();

        /*
         * timer
         * parma expireInterval milliseconds.
         */
        static int startTimer(timer_t *timerId, void (*Func)(int, siginfo_t*, void*), void* userData, long expireInterval, int repeat);
        static int stopTimer(timer_t timerId);
        static bool startsWith(const char* pre, const char* str);

    private:
    
};

#endif

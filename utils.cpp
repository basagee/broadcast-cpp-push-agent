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
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <net/if.h>

#include "constants.h"
#include "utils.h"

#include <iostream>
using namespace std;

int Utils::getMacAddress(unsigned char *macAddress) {
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];
    int success = 0;
    
    if (macAddress == NULL) {
	return success;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { 
	/* handle error*/ 
	return success;
    };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { 
	/* handle error */
	close(sock);
	return success;
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
            if (! (ifr.ifr_flags & IFF_LOOPBACK)) { // don't count loopback
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else { 
	    /* handle error */ 
	    continue;
	}
    }
    close(sock);
    
    if (success) {
        memcpy(macAddress, ifr.ifr_hwaddr.sa_data, 6);
    }
    
    return success;
}

int Utils::checkNetworkStatus() {
    struct ifconf ifc;
    char buf[1024];
    int success = 0;
    
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) { 
        /* handle error*/ 
        return success;
    };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) != -1) { 
        success = 1;
    }
    close(sock);
    return success;
}

#define SIGTIMER     (SIGRTMAX)
#define ONESHOTTIMER (SIGRTMAX-1)
#define ONE_MSEC_TO_NSEC        1000000
#define ONE_SEC_TO_NSEC         1000000000

int Utils::startTimer(timer_t* timerId, void (*Func)(int, siginfo_t*, void*), void* userData, long expireInterval, int repeat) {
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGTIMER;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = Func;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1) {
        log(ERROR, "Failed to setup signal handling\n");
        return(0);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = userData;
    
    int res = timer_create(CLOCK_REALTIME, &te, timerId);
    if (res != 0) {
	log(ERROR, "startTimer() failed timer_create errorno = %d\n", res);
	return 0;
    }

    long nano_intv = expireInterval * ONE_MSEC_TO_NSEC;
    its.it_value.tv_sec = nano_intv / ONE_SEC_TO_NSEC;
    its.it_value.tv_nsec = nano_intv % ONE_SEC_TO_NSEC;
    if (repeat) {
	its.it_interval.tv_sec = its.it_value.tv_sec;
	its.it_interval.tv_nsec = its.it_value.tv_nsec;
    } else {
	its.it_interval.tv_sec = 0;;
	its.it_interval.tv_nsec = 0;
    }
    
    struct itimerspec oitval;
    log(DEBUG, "startTimer is timerId = %d, repeat = %ld, tv_sec = %d, tv_nsec = %ld\n", *timerId, repeat, its.it_value.tv_sec, its.it_value.tv_nsec);
    res = timer_settime(*timerId, 0, &its, &oitval);
    if (res != 0) {
	log(ERROR, "startTimer() failed timer_settime errorno = %d\n", res);
	return 0;
    }

    return(1);
}

int Utils::stopTimer(timer_t timerId) {
    log(DEBUG, "stopTimer timerId = %d\n", timerId);
    timer_delete(timerId);
}

bool Utils::startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}


/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2022 Live Networks, Inc.  All rights reserved.
// Basic Usage Environment: for a simple, non-scripted, console application
// Implementation

#if(defined(__WIN32__) || defined(_WIN32)) && !defined(_WIN32_WINNT)
// WSAPoll is available starting with Windows Vista.
#define _WIN32_WINNT 0x0600
#endif

#include "BasicUsageEnvironment.hh"
#include "HandlerSet.hh"
#include <stdio.h>
#include <limits.h>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <vector>
#if defined(__WIN32__) || defined(_WIN32)
#include <winsock2.h>
#else
#include <poll.h>
#endif
#if defined(_QNX4)
#include <sys/select.h>
#include <unix.h>
#endif

////////// BasicTaskScheduler //////////

BasicTaskScheduler *BasicTaskScheduler::createNew(unsigned maxSchedulerGranularity) {
    return new BasicTaskScheduler(maxSchedulerGranularity);
}

BasicTaskScheduler::BasicTaskScheduler(unsigned maxSchedulerGranularity)
    : fMaxSchedulerGranularity(maxSchedulerGranularity) {
    if(maxSchedulerGranularity > 0)
        schedulerTickTask();  // ensures that we handle events frequently
}

BasicTaskScheduler::~BasicTaskScheduler() {}

void BasicTaskScheduler::schedulerTickTask(void *clientData) {
    ((BasicTaskScheduler *)clientData)->schedulerTickTask();
}

void BasicTaskScheduler::schedulerTickTask() {
    scheduleDelayedTask(fMaxSchedulerGranularity, schedulerTickTask, this);
}

#ifndef MILLION
#define MILLION 1000000
#endif

void BasicTaskScheduler::SingleStep(unsigned maxDelayTime) {
    DelayInterval const &timeToDelay = fDelayQueue.timeToNextAlarm();
    struct timeval       tv_timeToDelay;
    tv_timeToDelay.tv_sec  = timeToDelay.seconds();
    tv_timeToDelay.tv_usec = timeToDelay.useconds();
    // Keep the wait bounded to a duration supported by the polling API.
    // Don't make it any larger than 1 million seconds (11.5 days)
    const long MAX_TV_SEC = MILLION;
    if(tv_timeToDelay.tv_sec > MAX_TV_SEC) {
        tv_timeToDelay.tv_sec = MAX_TV_SEC;
    }
    // Also check our "maxDelayTime" parameter (if it's > 0):
    if(maxDelayTime > 0
       && (tv_timeToDelay.tv_sec > (long)maxDelayTime / MILLION
           || (tv_timeToDelay.tv_sec == (long)maxDelayTime / MILLION && tv_timeToDelay.tv_usec > (long)maxDelayTime % MILLION))) {
        tv_timeToDelay.tv_sec  = maxDelayTime / MILLION;
        tv_timeToDelay.tv_usec = maxDelayTime % MILLION;
    }

    const unsigned long long timeoutMs64 = static_cast<unsigned long long>(tv_timeToDelay.tv_sec) * 1000ULL
                                           + static_cast<unsigned long long>(tv_timeToDelay.tv_usec + 999) / 1000ULL;
    const int timeoutMs = timeoutMs64 > static_cast<unsigned long long>(INT_MAX) ? INT_MAX : static_cast<int>(timeoutMs64);

#if defined(__WIN32__) || defined(_WIN32)
    std::vector<WSAPOLLFD> pollFds;
#else
    std::vector<pollfd> pollFds;
#endif
    HandlerIterator pollIter(*fHandlers);
    HandlerDescriptor *pollHandler;
    while((pollHandler = pollIter.next()) != NULL) {
#if defined(__WIN32__) || defined(_WIN32)
        WSAPOLLFD pollFd = {};
#else
        pollfd pollFd = {};
#endif
        pollFd.fd = pollHandler->socketNum;
        if(pollHandler->conditionSet & SOCKET_READABLE) {
#if defined(__WIN32__) || defined(_WIN32)
            pollFd.events |= POLLRDNORM;
#else
            pollFd.events |= POLLIN;
#endif
        }
        if(pollHandler->conditionSet & SOCKET_WRITABLE)
            pollFd.events |= POLLOUT;
        if(pollHandler->conditionSet & SOCKET_EXCEPTION) {
#if defined(__WIN32__) || defined(_WIN32)
            // The Microsoft Winsock provider does not support POLLPRI. Use its
            // supported out-of-band-data event to preserve select semantics.
            pollFd.events |= POLLRDBAND;
#else
            pollFd.events |= POLLPRI;
#endif
        }
        pollFds.push_back(pollFd);
    }

#if defined(__WIN32__) || defined(_WIN32)
    int pollResult;
    int pollError = 0;
    if(pollFds.empty()) {
        if(timeoutMs > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        pollResult = 0;
    }
    else {
        pollResult = WSAPoll(pollFds.data(), static_cast<ULONG>(pollFds.size()), timeoutMs);
        pollError = pollResult < 0 ? WSAGetLastError() : 0;
    }
#else
    int pollResult = poll(pollFds.data(), pollFds.size(), timeoutMs);
    int pollError = pollResult < 0 ? errno : 0;
#endif
    if(pollResult < 0) {
        if(
#if defined(__WIN32__) || defined(_WIN32)
           pollError != WSAEINTR
#else
           pollError != EINTR
           && pollError != EAGAIN
#endif
        ) {
            // Unexpected error - treat this as fatal:
#if !defined(_WIN32_WCE)
            perror("BasicTaskScheduler::SingleStep(): poll() fails");
#endif
            internalError();
        }
    }

    std::unordered_map<int, short> readyEvents;
    if(pollResult > 0) {
        for(const auto &pollFd: pollFds) {
            if(pollFd.revents != 0)
                readyEvents[pollFd.fd] = pollFd.revents;
        }
    }

    auto getResultConditionSet = [&readyEvents](HandlerDescriptor *handler) {
        const auto it = readyEvents.find(handler->socketNum);
        if(it == readyEvents.end())
            return 0;
        const short revents = it->second;
        int resultConditionSet = 0;
#if defined(__WIN32__) || defined(_WIN32)
        if(revents & POLLRDNORM)
#else
        if(revents & POLLIN)
#endif
            resultConditionSet |= SOCKET_READABLE;
        if(revents & POLLOUT)
            resultConditionSet |= SOCKET_WRITABLE;
#if defined(__WIN32__) || defined(_WIN32)
        if(revents & POLLRDBAND)
#else
        if(revents & POLLPRI)
#endif
            resultConditionSet |= SOCKET_EXCEPTION;
        // A closed or errored socket must still reach the registered handler so
        // handler using its requested condition so existing error/EOF handling runs.
        if(revents & (POLLERR | POLLHUP | POLLNVAL))
            resultConditionSet |= handler->conditionSet;
        return resultConditionSet;
    };

    // Call the handler function for one readable socket:
    HandlerIterator    iter(*fHandlers);
    HandlerDescriptor *handler;
    // To ensure forward progress through the handlers, begin past the last
    // socket number that we handled:
    if(fLastHandledSocketNum >= 0) {
        while((handler = iter.next()) != NULL) {
            if(handler->socketNum == fLastHandledSocketNum)
                break;
        }
        if(handler == NULL) {
            fLastHandledSocketNum = -1;
            iter.reset();  // start from the beginning instead
        }
    }
    while((handler = iter.next()) != NULL) {
        int sock               = handler->socketNum;  // alias
        int resultConditionSet = getResultConditionSet(handler);
        if((resultConditionSet & handler->conditionSet) != 0 && handler->handlerProc != NULL) {
            fLastHandledSocketNum = sock;
            // Note: we set "fLastHandledSocketNum" before calling the handler,
            // in case the handler calls "doEventLoop()" reentrantly.
            (*handler->handlerProc)(handler->clientData, resultConditionSet);
            break;
        }
    }
    if(handler == NULL && fLastHandledSocketNum >= 0) {
        // We didn't call a handler, but we didn't get to check all of them,
        // so try again from the beginning:
        iter.reset();
        while((handler = iter.next()) != NULL) {
            int sock               = handler->socketNum;  // alias
            int resultConditionSet = getResultConditionSet(handler);
            if((resultConditionSet & handler->conditionSet) != 0 && handler->handlerProc != NULL) {
                fLastHandledSocketNum = sock;
                // Note: we set "fLastHandledSocketNum" before calling the handler,
                // in case the handler calls "doEventLoop()" reentrantly.
                (*handler->handlerProc)(handler->clientData, resultConditionSet);
                break;
            }
        }
        if(handler == NULL)
            fLastHandledSocketNum = -1;  // because we didn't call a handler
    }

    // Also handle any newly-triggered event (Note that we do this *after* calling a socket handler,
    // in case the triggered event handler modifies The set of readable sockets.)
    if(fTriggersAwaitingHandling != 0) {
        if(fTriggersAwaitingHandling == fLastUsedTriggerMask) {
            // Common-case optimization for a single event trigger:
            fTriggersAwaitingHandling &= ~fLastUsedTriggerMask;
            if(fTriggeredEventHandlers[fLastUsedTriggerNum] != NULL) {
                (*fTriggeredEventHandlers[fLastUsedTriggerNum])(fTriggeredEventClientDatas[fLastUsedTriggerNum]);
            }
        }
        else {
            // Look for an event trigger that needs handling (making sure that we make forward progress through all possible triggers):
            unsigned       i    = fLastUsedTriggerNum;
            EventTriggerId mask = fLastUsedTriggerMask;

            do {
                i = (i + 1) % MAX_NUM_EVENT_TRIGGERS;
                mask >>= 1;
                if(mask == 0)
                    mask = 0x80000000;

                if((fTriggersAwaitingHandling & mask) != 0) {
                    fTriggersAwaitingHandling &= ~mask;
                    if(fTriggeredEventHandlers[i] != NULL) {
                        (*fTriggeredEventHandlers[i])(fTriggeredEventClientDatas[i]);
                    }

                    fLastUsedTriggerMask = mask;
                    fLastUsedTriggerNum  = i;
                    break;
                }
            } while(i != fLastUsedTriggerNum);
        }
    }

    // Also handle any delayed event that may have come due.
    fDelayQueue.handleAlarm();
}

void BasicTaskScheduler ::setBackgroundHandling(int socketNum, int conditionSet, BackgroundHandlerProc *handlerProc, void *clientData) {
    if(socketNum < 0)
        return;
    if(conditionSet == 0) {
        fHandlers->clearHandler(socketNum);
    }
    else {
        fHandlers->assignHandler(socketNum, conditionSet, handlerProc, clientData);
    }
}

void BasicTaskScheduler::moveSocketHandling(int oldSocketNum, int newSocketNum) {
    if(oldSocketNum < 0 || newSocketNum < 0)
        return;  // sanity check
    fHandlers->moveHandler(oldSocketNum, newSocketNum);
}

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#if(defined(WIN32) || defined(_WIN32)) && !defined(_WIN32_WINNT)
// WSAPoll is available starting with Windows Vista.
#define _WIN32_WINNT 0x0600
#endif

#include "SocketPoller.hpp"

#include <chrono>
#include <climits>
#include <thread>

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
#include <winsock2.h>
#else
#include <poll.h>
#endif

namespace libobsensor {

namespace {

short toNativeEvents(short events) {
    short nativeEvents = 0;
    if(events & SOCKET_POLL_EVENT_READ) {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        nativeEvents |= POLLRDNORM;
#else
        nativeEvents |= POLLIN;
#endif
    }
    if(events & SOCKET_POLL_EVENT_WRITE) {
        nativeEvents |= POLLOUT;
    }
    if(events & SOCKET_POLL_EVENT_EXCEPTION) {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        // The Microsoft Winsock provider does not support POLLPRI. Its
        // supported out-of-band-data event is POLLRDBAND instead.
        nativeEvents |= POLLRDBAND;
#else
        nativeEvents |= POLLPRI;
#endif
    }
    return nativeEvents;
}

short fromNativeEvents(short nativeEvents) {
    short events = 0;
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    if(nativeEvents & POLLRDNORM) {
#else
    if(nativeEvents & POLLIN) {
#endif
        events |= SOCKET_POLL_EVENT_READ;
    }
    if(nativeEvents & POLLOUT) {
        events |= SOCKET_POLL_EVENT_WRITE;
    }
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
    if(nativeEvents & POLLRDBAND) {
#else
    if(nativeEvents & POLLPRI) {
#endif
        events |= SOCKET_POLL_EVENT_EXCEPTION;
    }
    if(nativeEvents & POLLERR) {
        events |= SOCKET_POLL_EVENT_ERROR;
    }
    if(nativeEvents & POLLHUP) {
        events |= SOCKET_POLL_EVENT_HANGUP;
    }
#ifdef POLLRDHUP
    if(nativeEvents & POLLRDHUP) {
        events |= SOCKET_POLL_EVENT_HANGUP;
    }
#endif
    if(nativeEvents & POLLNVAL) {
        events |= SOCKET_POLL_EVENT_INVALID;
    }
    return events;
}

}  // namespace

int waitForSocketEvents(std::vector<SocketPollFd> &fds, int timeoutMs) {
    // WSAPoll does not consistently accept an empty descriptor array. Match
    // poll's timeout behavior explicitly so callers do not spin while idle.
    if(fds.empty()) {
        const int idleTimeoutMs = timeoutMs < 0 ? INT_MAX : timeoutMs;
        if(idleTimeoutMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(idleTimeoutMs));
        }
        return 0;
    }

    int result      = 0;
    int socketError = 0;
    {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        std::vector<WSAPOLLFD> nativeFds(fds.size());
#else
        std::vector<pollfd> nativeFds(fds.size());
#endif

        for(size_t i = 0; i < fds.size(); ++i) {
            fds[i].revents       = 0;
            nativeFds[i].fd      = fds[i].fd;
            nativeFds[i].events  = toNativeEvents(fds[i].events);
            nativeFds[i].revents = 0;
        }

#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        result = WSAPoll(nativeFds.data(), static_cast<ULONG>(nativeFds.size()), timeoutMs);
        if(result < 0) {
            socketError = WSAGetLastError();
        }
#else
        result = poll(nativeFds.data(), nativeFds.size(), timeoutMs);
        if(result < 0) {
            socketError = errno;
        }
#endif

        if(result > 0) {
            for(size_t i = 0; i < fds.size(); ++i) {
                fds[i].revents = fromNativeEvents(nativeFds[i].revents);
            }
        }
    }

    if(result < 0) {
#if(defined(WIN32) || defined(_WIN32) || defined(WINCE))
        WSASetLastError(socketError);
#else
        errno = socketError;
#endif
    }
    return result;
}

}  // namespace libobsensor

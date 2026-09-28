// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "SocketTypes.hpp"

#include <vector>

namespace libobsensor {

enum SocketPollEvent : short {
    SOCKET_POLL_EVENT_READ      = 1 << 0,
    SOCKET_POLL_EVENT_WRITE     = 1 << 1,
    SOCKET_POLL_EVENT_EXCEPTION = 1 << 2,
    SOCKET_POLL_EVENT_ERROR     = 1 << 3,
    SOCKET_POLL_EVENT_HANGUP    = 1 << 4,
    SOCKET_POLL_EVENT_INVALID   = 1 << 5,
};

struct SocketPollFd {
    SOCKET fd;
    short  events;
    short  revents;

    SocketPollFd(SOCKET socketFd, short requestedEvents) : fd(socketFd), events(requestedEvents), revents(0) {}
};

// Wait for socket events without the FD_SETSIZE limitation of select().
// timeoutMs follows poll/WSAPoll semantics: 0 does not wait and -1 waits forever.
int waitForSocketEvents(std::vector<SocketPollFd> &fds, int timeoutMs);

}  // namespace libobsensor

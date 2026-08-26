// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HostTimestampProvider.hpp"
#include "frame/Frame.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"

#include <chrono>
#include <ctime>

#if !defined(_WIN32) && !defined(__GLIBC__)
// glibc already declares timegm in <time.h> (with throw()); redeclaring it here
// would conflict on its exception specifier. Only declare as a fallback for
// libc implementations that do not expose it.
extern "C" time_t timegm(struct tm *timeptr);
#endif

namespace libobsensor {

namespace {

uint64_t getLocalTimeUs(uint64_t utcUs) {
    std::time_t utcTime = static_cast<std::time_t>(utcUs / 1000000);
    std::tm     localTm{};

#ifdef _WIN32
    localtime_s(&localTm, &utcTime);
#else
    localtime_r(&utcTime, &localTm);
#endif

    auto localTime =
#ifdef _WIN32
        _mkgmtime(&localTm);
#else
        timegm(&localTm);
#endif
    if(localTime == static_cast<std::time_t>(-1)) {
        return 0;
    }

    return static_cast<uint64_t>(localTime) * 1000000 + utcUs % 1000000;
}

}  // namespace

HostTimestampProvider ::HostTimestampProvider() {
    loadConfig();
}

void HostTimestampProvider::loadConfig() {
    auto        envConfig = EnvConfig::getInstance();
    std::string value;
    auto        type = OB_CLOCK_TYPE_REALTIME;  // default

    if(envConfig->getStringValue("Device.ClockSource", value)) {
        if(value == "Realtime") {
            type = OB_CLOCK_TYPE_REALTIME;
        }
        else if(value == "Monotonic") {
            type = OB_CLOCK_TYPE_MONOTONIC;
        }
        else {
            LOG_DEBUG("Unknown clock source, use realtime instead");
        }
    }
    setClockType(type);
}

void HostTimestampProvider::setClockType(OBClockType type) {
    clockType_.store(type, std::memory_order_relaxed);
    LOG_INFO("Update clock type to: {}", type);
}

OBClockType HostTimestampProvider::getClockType() const {
    return clockType_.load(std::memory_order_relaxed);
}

uint64_t HostTimestampProvider::getTimeUs(OBTimestampTimeBase timeBase) const {
    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        return utils::getSteadyTimeUs();
    }

    if(timeBase == OB_TIMESTAMP_TIME_BASE_LOCAL) {
        auto tp = std::chrono::system_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
        return getLocalTimeUs(static_cast<uint64_t>(us));
    }
    return utils::getNowTimesUs();
}

uint64_t HostTimestampProvider::getTimeMs(OBTimestampTimeBase timeBase) const {
    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        return utils::getSteadyTimeMs();
    }

    return getTimeUs(timeBase) / 1000;
}

void HostTimestampProvider::applyHostTimestamp(const std::shared_ptr<Frame> &frame, OBTimestampTimeBase timeBase) const {
    if(!frame) {
        return;
    }

    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        // Select steady time as frame system time
        auto steadyTime = frame->getSteadyTimeStampUsec();
        frame->setSystemTimeStampUsec(steadyTime);
        if(frame->isDeviceTimestampFromHost()) {
            frame->setTimeStampUsec(steadyTime);
        }
        return;
    }

    if(timeBase == OB_TIMESTAMP_TIME_BASE_LOCAL) {
        auto systemTime = frame->getSystemTimeStampUsec();
        if(systemTime == 0) {
            systemTime = utils::getNowTimesUs();
        }

        auto localTime = getLocalTimeUs(systemTime);
        if(localTime > 0) {
            frame->setSystemTimeStampUsec(localTime);
            if(frame->isDeviceTimestampFromHost()) {
                frame->setTimeStampUsec(localTime);
            }
        }
    }
}

}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "HostTimestampProvider.hpp"
#include "frame/Frame.hpp"
#include "utils/Utils.hpp"
#include "environment/EnvConfig.hpp"

#include <ctime>

namespace libobsensor {

namespace {

int64_t getLocalTimezoneOffsetUs() {
    std::time_t now = std::time(nullptr);
    std::tm     utcTm{};
    std::tm     localTm{};

#ifdef _WIN32
    gmtime_s(&utcTm, &now);
    localtime_s(&localTm, &now);
#else
    gmtime_r(&now, &utcTm);
    localtime_r(&now, &localTm);
#endif

    auto utcAsLocalEpoch   = std::mktime(&utcTm);
    auto localAsLocalEpoch = std::mktime(&localTm);
    if(utcAsLocalEpoch == static_cast<std::time_t>(-1) || localAsLocalEpoch == static_cast<std::time_t>(-1)) {
        return 0;
    }

    return static_cast<int64_t>(std::difftime(localAsLocalEpoch, utcAsLocalEpoch) * 1000000.0);
}

}  // namespace

HostTimestampProvider ::HostTimestampProvider() {
    loadConfig();
}

void HostTimestampProvider::loadConfig() {
    auto        envConfig = EnvConfig::getInstance();
    std::string value;
    auto        type = OB_CLOCK_TYPE_REALTIME;  // default
    auto        timeBase = OB_TIMESTAMP_TIME_BASE_UTC;

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

    if(envConfig->getStringValue("Device.TimestampTimeBase", value) || envConfig->getStringValue("Device.DeviceTimeSyncTimeBase", value)) {
        if(value == "UTC") {
            timeBase = OB_TIMESTAMP_TIME_BASE_UTC;
        }
        else if(value == "Local") {
            timeBase = OB_TIMESTAMP_TIME_BASE_LOCAL;
        }
        else {
            LOG_DEBUG("Unknown timestamp time base, use UTC instead");
        }
    }
    setTimestampTimeBase(timeBase);
}

void HostTimestampProvider::setClockType(OBClockType type) {
    clockType_.store(type, std::memory_order_relaxed);
    LOG_INFO("Update clock type to: {}", type);
}

OBClockType HostTimestampProvider::getClockType() const {
    return clockType_.load(std::memory_order_relaxed);
}

void HostTimestampProvider::setTimestampTimeBase(OBTimestampTimeBase timeBase) {
    timestampTimeBase_.store(timeBase, std::memory_order_relaxed);
    LOG_INFO("Update timestamp time base to: {}", static_cast<int>(timeBase));
}

OBTimestampTimeBase HostTimestampProvider::getTimestampTimeBase() const {
    return timestampTimeBase_.load(std::memory_order_relaxed);
}

uint64_t HostTimestampProvider::getTimeUs() const {
    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        return utils::getSteadyTimeUs();
    }

    auto utcUs = utils::getNowTimesUs();
    if(getTimestampTimeBase() == OB_TIMESTAMP_TIME_BASE_LOCAL) {
        auto localUs = static_cast<int64_t>(utcUs) + getLocalTimezoneOffsetUs();
        return localUs > 0 ? static_cast<uint64_t>(localUs) : 0;
    }
    return utcUs;
}

uint64_t HostTimestampProvider::getTimeMs() const {
    if(getClockType() == OB_CLOCK_TYPE_MONOTONIC) {
        return utils::getSteadyTimeMs();
    }

    return getTimeUs() / 1000;
}

void HostTimestampProvider::applyHostTimestamp(const std::shared_ptr<Frame> &frame) const {
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

    if(getTimestampTimeBase() == OB_TIMESTAMP_TIME_BASE_LOCAL) {
        auto systemTime = frame->getSystemTimeStampUsec();
        if(systemTime == 0) {
            systemTime = utils::getNowTimesUs();
        }

        auto localTime = static_cast<int64_t>(systemTime) + getLocalTimezoneOffsetUs();
        if(localTime > 0) {
            frame->setSystemTimeStampUsec(static_cast<uint64_t>(localTime));
            if(frame->isDeviceTimestampFromHost()) {
                frame->setTimeStampUsec(static_cast<uint64_t>(localTime));
            }
        }
    }
}

}  // namespace libobsensor

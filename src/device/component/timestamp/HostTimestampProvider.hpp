// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "libobsensor/h/ObTypes.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace libobsensor {

class Frame;

/**
 * @brief Host timestamp provider.
 */
class HostTimestampProvider {
public:
    HostTimestampProvider();

    /**
     * @brief Set the current host-side clock type.
     *
     * @param type Target clock type.
     */
    void setClockType(OBClockType type);

    /**
     * @brief Get the current host-side clock type.
     *
     * @return Current clock type.
     */
    OBClockType getClockType() const;

    /**
     * @brief Set the time base used for realtime timestamps.
     *
     * @param timeBase Target timestamp time base.
     */
    void setTimestampTimeBase(OBTimestampTimeBase timeBase);

    /**
     * @brief Get the time base used for realtime timestamps.
     *
     * @return Current timestamp time base.
     */
    OBTimestampTimeBase getTimestampTimeBase() const;

    /**
     * @brief Get current host time in microseconds for the selected clock type.
     *
     * @return Current time in microseconds.
     */
    uint64_t getTimeUs() const;

    /**
     * @brief Get current host time in milliseconds for the selected clock type.
     *
     * @return Current time in milliseconds.
     */
    uint64_t getTimeMs() const;

    /**
     * @brief Apply frame timestamp.
     *
     * @param frame Target frame.
     */
    void applyHostTimestamp(const std::shared_ptr<Frame> &frame) const;

private:
    void loadConfig();

private:
    std::atomic<OBClockType> clockType_{ OB_CLOCK_TYPE_REALTIME };
    std::atomic<OBTimestampTimeBase> timestampTimeBase_{ OB_TIMESTAMP_TIME_BASE_UTC };
};

}  // namespace libobsensor

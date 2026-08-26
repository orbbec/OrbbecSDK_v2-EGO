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
     * @brief Get current host time in microseconds using an explicit time base.
     *
     * @param timeBase Time base to apply for the realtime clock.
     * @return Current time in microseconds.
     */
    uint64_t getTimeUs(OBTimestampTimeBase timeBase) const;

    /**
     * @brief Get current host time in milliseconds using an explicit time base.
     *
     * @param timeBase Time base to apply for the realtime clock.
     * @return Current time in milliseconds.
     */
    uint64_t getTimeMs(OBTimestampTimeBase timeBase) const;

    /**
     * @brief Apply frame timestamp using an explicit time base.
     *
     * @param frame Target frame.
     * @param timeBase Time base to apply for the realtime clock.
     */
    void applyHostTimestamp(const std::shared_ptr<Frame> &frame, OBTimestampTimeBase timeBase) const;

private:
    void loadConfig();

private:
    std::atomic<OBClockType> clockType_{ OB_CLOCK_TYPE_REALTIME };
};

}  // namespace libobsensor

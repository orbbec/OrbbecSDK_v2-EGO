// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "component/DeviceComponentBase.hpp"

#include <cstdint>

namespace libobsensor {

class UtcOffsetManager : public DeviceComponentBase {
public:
    explicit UtcOffsetManager(IDevice *owner);
    ~UtcOffsetManager() noexcept override = default;

    // Calculates the current local civil-time offset from UTC and writes it to the device.
    void syncFromSystem();

private:
    static int32_t getCurrentLocalUtcOffsetMinutes();
};

}  // namespace libobsensor

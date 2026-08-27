// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "component/DeviceComponentBase.hpp"
#include "InternalTypes.hpp"

#include <cstdint>
#include <ctime>
#include <string>

namespace libobsensor {

class HostTimeInfoManager : public DeviceComponentBase {
public:
    explicit HostTimeInfoManager(IDevice *owner);
    ~HostTimeInfoManager() noexcept override = default;

    // Writes the current host UTC offset to the legacy device property.
    void setUtcOffsetToDevice();

    // Writes the current host UTC time, UTC offset and IANA time zone to the device.
    void setHostTimeInfoToDevice();

private:
    static int32_t       getUtcOffsetMinutes(std::time_t timestamp);
    static std::string   getCurrentIanaTimeZone();
    static OBHostTimeInfo buildHostTimeInfo();
};

}  // namespace libobsensor

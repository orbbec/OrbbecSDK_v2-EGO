// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "ISourcePort.hpp"
#include <cstdint>
namespace libobsensor {
enum class ScsiDataDirection { Out, In };
class IScsiDevicePort : public ISourcePort {
public:
    ~IScsiDevicePort() noexcept override                                                                                                         = default;
    virtual void execute(const uint8_t *cdb, uint8_t cdbSize, ScsiDataDirection direction, uint8_t *data, uint32_t dataSize, uint32_t timeoutMs) = 0;
};
}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"

namespace libobsensor {

class IEGOProColorDemuxer : public IDeviceComponent {
public:
    ~IEGOProColorDemuxer() noexcept override = default;
    virtual std::shared_ptr<IVideoStreamPort> createVirtualPort(uint8_t colorId) = 0;
};

}  // namespace libobsensor

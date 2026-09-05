// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"

#include <string>

namespace libobsensor {

// Common device-level contract for audio playback transports.  Concrete
// implementations deliberately retain their own transport timing/state.
class IAudioPlayer : public IDeviceComponent {
public:
    ~IAudioPlayer() noexcept override = default;

    virtual void playFile(const std::string &filePath) = 0;
    virtual void stop()                                = 0;
};

}  // namespace libobsensor

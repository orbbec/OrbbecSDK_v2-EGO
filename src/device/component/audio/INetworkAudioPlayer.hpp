// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"

#include <string>

namespace libobsensor {

class INetworkAudioPlayer : public IDeviceComponent {
public:
    ~INetworkAudioPlayer() noexcept override = default;

    virtual void playFile(const std::string &filePath) = 0;
    virtual void stop()                                           = 0;
};

}  // namespace libobsensor

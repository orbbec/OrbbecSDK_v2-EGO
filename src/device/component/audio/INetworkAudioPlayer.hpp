// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IAudioPlayer.hpp"

#include <string>

namespace libobsensor {

class INetworkAudioPlayer : public IAudioPlayer {
public:
    ~INetworkAudioPlayer() noexcept override = default;

    void playFile(const std::string &filePath) override = 0;
    void stop() override                                = 0;
};

}  // namespace libobsensor

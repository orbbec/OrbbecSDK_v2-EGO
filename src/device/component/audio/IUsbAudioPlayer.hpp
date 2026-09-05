// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IAudioPlayer.hpp"

namespace libobsensor {

class IUsbAudioPlayer : public IAudioPlayer {
public:
    ~IUsbAudioPlayer() noexcept override = default;
};

}  // namespace libobsensor

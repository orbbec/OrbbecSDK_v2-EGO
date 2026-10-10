// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace libobsensor {

// Optional backend capability for device classes to provide the audio channel count
// specified by their stream contract before the sensor queries its stream profiles.
class IAudioChannelCountConfigurable {
public:
    virtual ~IAudioChannelCountConfigurable() = default;
    virtual void setAudioChannelCount(uint32_t channelCount) = 0;
};

}  // namespace libobsensor

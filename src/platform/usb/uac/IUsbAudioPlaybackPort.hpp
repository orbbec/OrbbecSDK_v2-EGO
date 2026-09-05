// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"

#include <cstddef>
#include <cstdint>

namespace libobsensor {

class IUsbAudioPlaybackPort : public ISourcePort {
public:
    ~IUsbAudioPlaybackPort() noexcept override = default;

    virtual void   openSession() = 0;
    virtual void   beginPlayback(uint64_t generation) = 0;
    virtual size_t writePcm(const uint8_t *data, size_t size, uint64_t generation) = 0;
    // Returns false when playback was interrupted or the transport failed before draining.
    virtual bool   drain(uint64_t generation) = 0;
    virtual void   endPlayback(uint64_t generation) noexcept = 0;
    virtual void   closeSession() noexcept = 0;
};

}  // namespace libobsensor

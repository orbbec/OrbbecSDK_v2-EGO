// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ISourcePort.hpp"
#include "IDeviceComponent.hpp"
#include "IStreamer.hpp"

#include <atomic>
#include <map>
#include <mutex>

namespace libobsensor {

// Streamer for the MT6826S magnetic angle encoder. Unlike ImuStreamer (reportId=1, DMC640 group payload),
// the encoder is a self-contained record on its own HID report id (reportId=5) and is parsed independently.
class MagEncoderStreamer : public IDeviceComponent, public IStreamer {
public:
    MagEncoderStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~MagEncoderStreamer() noexcept override;

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void stopStream(std::shared_ptr<const StreamProfile> profile) override;

    IDevice *getOwner() const override;

private:
    void parseMagEncoderData(std::shared_ptr<Frame> frame);

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex                                                           cbMtx_;
    std::map<std::shared_ptr<const StreamProfile>, MutableFrameCallback> callbacks_;

    std::atomic_bool running_;
    uint64_t         frameIndex_;
};
}  // namespace libobsensor

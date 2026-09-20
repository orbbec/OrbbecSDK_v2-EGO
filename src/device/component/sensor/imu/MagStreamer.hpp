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

// Streamer for the magnetometer. Unlike ImuStreamer (reportId=1, accel/gyro group payload), the
// magnetometer is a self-contained record on its own HID report id (reportId=6) and is parsed
// independently. The raw int16 magX/Y/Z values are converted to uT using the full-scale range
// carried by the active MagStreamProfile.
class MagStreamer : public IDeviceComponent, public IStreamer {
public:
    MagStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend);
    ~MagStreamer() noexcept override;

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void stopStream(std::shared_ptr<const StreamProfile> profile) override;

    IDevice *getOwner() const override;

private:
    void parseMagData(std::shared_ptr<Frame> frame);

private:
    IDevice                         *owner_;
    std::shared_ptr<IDataStreamPort> backend_;

    std::mutex                                                           cbMtx_;
    std::map<std::shared_ptr<const StreamProfile>, MutableFrameCallback> callbacks_;

    std::atomic_bool running_;
    uint64_t         frameIndex_;
};
}  // namespace libobsensor

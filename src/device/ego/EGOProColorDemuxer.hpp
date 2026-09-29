// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"
#include "EGOProColorDemuxerInterface.hpp"

#include <array>
#include <map>
#include <memory>
#include <mutex>

namespace libobsensor {

class EGOProColorDemuxer : public IEGOProColorDemuxer, public std::enable_shared_from_this<EGOProColorDemuxer> {
public:
    EGOProColorDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort);
    ~EGOProColorDemuxer() noexcept override;

    IDevice                          *getOwner() const override;
    std::shared_ptr<IVideoStreamPort> createVirtualPort(uint8_t colorId) override;

    void                                  startStream(uint8_t colorId, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void                                  stopStream(uint8_t colorId);
    void                                  stopAllStream();
    StreamProfileList                     getStreamProfileList();
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const;

private:
    bool isSameProfile(const std::shared_ptr<const StreamProfile> &lhs, const std::shared_ptr<const StreamProfile> &rhs) const;
    void onFrame(std::shared_ptr<Frame> frame);
    // EgoPro multiplexes 4 color channels onto one shared UVC endpoint, so the firmware
    // STOP_COLOR_STREAM command stops the whole shared color pipeline (not a single channel).
    // Send it on the real UVC port only when that endpoint actually closes (last channel stopped).
    void sendStopStreamVendorCmd();

private:
    IDevice                                *owner_;
    std::shared_ptr<IVideoStreamPort>       realPort_;
    mutable std::mutex                      mutex_;
    std::map<uint8_t, MutableFrameCallback> callbacks_;
    // The physical UVC endpoint alternates frames from all four cameras.  Keep
    // an independent sequence for every virtual color stream instead.
    std::array<uint64_t, 4>              frameIndexes_{};
    std::shared_ptr<const StreamProfile> activeProfile_;
};

}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "EGOProColorDemuxerInterface.hpp"
#include "ISourcePort.hpp"

#include <array>
#include <map>
#include <memory>
#include <mutex>

namespace libobsensor {

class EGOProEC130ColorDemuxer : public IEGOProColorDemuxer, public std::enable_shared_from_this<EGOProEC130ColorDemuxer> {
public:
    EGOProEC130ColorDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort);
    ~EGOProEC130ColorDemuxer() noexcept override;

    IDevice *getOwner() const override;
    std::shared_ptr<IVideoStreamPort> createVirtualPort(uint8_t colorId);
    void startStream(uint8_t colorId, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void stopStream(uint8_t colorId);
    void stopAllStream();
    StreamProfileList getStreamProfileList(uint8_t colorId);
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const;

private:
    bool isSameProfile(const std::shared_ptr<const StreamProfile> &lhs, const std::shared_ptr<const StreamProfile> &rhs) const;
    void onFrame(std::shared_ptr<Frame> frame);
    void sendStopStreamVendorCmd();

    IDevice *owner_;
    std::shared_ptr<IVideoStreamPort> realPort_;
    mutable std::mutex mutex_;
    std::map<uint8_t, MutableFrameCallback> callbacks_;
    std::map<uint8_t, std::shared_ptr<const StreamProfile>> channelProfiles_;
    std::array<uint64_t, 4> frameIndexes_{};
    std::shared_ptr<const StreamProfile> activeProfile_;
};

}  // namespace libobsensor

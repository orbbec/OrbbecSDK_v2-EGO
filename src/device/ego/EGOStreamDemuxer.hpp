// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"
#include "ISourcePort.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace libobsensor {

// Configuration for a shared-endpoint stream demuxer.
//
// EGODepth multiplexes several logical streams onto one physical UVC endpoint (e.g. 3 Color cameras
// on one endpoint, 2 IR cameras on another). Each frame carries a cam_id in its UVC SCR field; the
// demuxer routes it to the matching virtual stream. This struct describes one such group:
//   - camIdToFrameType : maps the per-frame cam_id to the logical OBFrameType of that channel.
//   - stopStreamPropertyId : firmware vendor command used to stop the whole shared pipeline once the
//                            last active channel of the group is closed (-1 disables the command).
//   - vendorCmdPort : port that carries the vendor XU for the stop command. EGODepth exposes a single
//                     XU (on the Color-group interface), so the IR group must send its stop command on
//                     that interface rather than its own data endpoint. When null the demuxer falls back
//                     to its own real data port (the EgoPro / Color-group case, where the XU rides the
//                     same interface as the data endpoint).
struct EGODemuxConfig {
    std::map<uint8_t, OBFrameType> camIdToFrameType;
    int                            stopStreamPropertyId = -1;
    std::shared_ptr<ISourcePort>   vendorCmdPort;
};

class EGOStreamDemuxer : public IDeviceComponent, public std::enable_shared_from_this<EGOStreamDemuxer> {
public:
    EGOStreamDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort, EGODemuxConfig config);
    ~EGOStreamDemuxer() noexcept override;

    IDevice                          *getOwner() const override;
    std::shared_ptr<IVideoStreamPort> createVirtualPort(uint8_t camId);

    void                                  startStream(uint8_t camId, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback);
    void                                  stopStream(uint8_t camId);
    void                                  stopAllStream();
    StreamProfileList                     getStreamProfileList();
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const;

private:
    bool        isValidCamId(uint8_t camId) const;
    bool        isSameProfile(const std::shared_ptr<const StreamProfile> &lhs, const std::shared_ptr<const StreamProfile> &rhs) const;
    OBFrameType mapCamIdToFrameType(uint8_t camId) const;
    void        onFrame(std::shared_ptr<Frame> frame);
    // The channels of a group share one UVC endpoint, so the firmware stop-stream command stops the
    // whole shared pipeline (not a single channel). Send it on the real UVC port only when that
    // endpoint actually closes (last channel stopped).
    void sendStopStreamVendorCmd();

private:
    IDevice                                *owner_;
    std::shared_ptr<IVideoStreamPort>       realPort_;
    EGODemuxConfig                          config_;
    mutable std::mutex                      mutex_;
    std::map<uint8_t, MutableFrameCallback> callbacks_;
    // The physical UVC endpoint alternates frames from all channels; keep an independent sequence
    // for every virtual stream instead of a single shared counter.
    std::map<uint8_t, uint64_t>          frameIndexes_;
    std::shared_ptr<const StreamProfile> activeProfile_;
};

}  // namespace libobsensor

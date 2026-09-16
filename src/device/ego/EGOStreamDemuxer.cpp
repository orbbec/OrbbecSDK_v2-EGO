// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOStreamDemuxer.hpp"

#include "exception/ObException.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "logger/LoggerInterval.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "usb/uvc/UvcTypes.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "component/property/InternalProperty.hpp"
#include "utils/PublicTypeHelper.hpp"

#include <string>

namespace libobsensor {

namespace {
constexpr size_t EGO_SCR_SOURCE_CLOCK_LOW_OFFSET = 4;

uint8_t getEgoCamId(const StandardUvcFramePayloadHeader *metadata) {
    const auto scrSof = static_cast<uint16_t>(metadata->scrSourceClock[EGO_SCR_SOURCE_CLOCK_LOW_OFFSET])
                        | (static_cast<uint16_t>(metadata->scrSourceClock[EGO_SCR_SOURCE_CLOCK_LOW_OFFSET + 1]) << 8);
    return static_cast<uint8_t>((scrSof >> 11) & 0x07);
}

class EGOStreamVirtualPort : public IVideoStreamPort {
public:
    EGOStreamVirtualPort(std::shared_ptr<EGOStreamDemuxer> demuxer, uint8_t camId) : demuxer_(std::move(demuxer)), camId_(camId) {}

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return demuxer_->getSourcePortInfo();
    }

    StreamProfileList getStreamProfileList() override {
        return demuxer_->getStreamProfileList();
    }

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override {
        demuxer_->startStream(camId_, std::move(profile), std::move(callback));
    }

    void stopStream(std::shared_ptr<const StreamProfile> profile) override {
        (void)profile;
        demuxer_->stopStream(camId_);
    }

    void stopAllStream() override {
        demuxer_->stopStream(camId_);
    }

private:
    std::shared_ptr<EGOStreamDemuxer> demuxer_;
    uint8_t                           camId_;
};
}  // namespace

EGOStreamDemuxer::EGOStreamDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort, EGODemuxConfig config)
    : owner_(owner), realPort_(std::move(realPort)), config_(std::move(config)) {
    if(!realPort_) {
        THROW_INVALID_PARAM_EXCEPTION("EGO stream demuxer requires a real video stream port");
    }
    if(config_.camIdToFrameType.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("EGO stream demuxer requires at least one cam id mapping");
    }
}

EGOStreamDemuxer::~EGOStreamDemuxer() noexcept {
    TRY_EXECUTE(stopAllStream());
}

IDevice *EGOStreamDemuxer::getOwner() const {
    return owner_;
}

bool EGOStreamDemuxer::isValidCamId(uint8_t camId) const {
    return config_.camIdToFrameType.find(camId) != config_.camIdToFrameType.end();
}

OBFrameType EGOStreamDemuxer::mapCamIdToFrameType(uint8_t camId) const {
    auto iter = config_.camIdToFrameType.find(camId);
    return iter != config_.camIdToFrameType.end() ? iter->second : OB_FRAME_UNKNOWN;
}

std::shared_ptr<IVideoStreamPort> EGOStreamDemuxer::createVirtualPort(uint8_t camId) {
    if(!isValidCamId(camId)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO stream cam id");
    }
    return std::make_shared<EGOStreamVirtualPort>(shared_from_this(), camId);
}

StreamProfileList EGOStreamDemuxer::getStreamProfileList() {
    return realPort_->getStreamProfileList();
}

std::shared_ptr<const SourcePortInfo> EGOStreamDemuxer::getSourcePortInfo() const {
    return realPort_->getSourcePortInfo();
}

bool EGOStreamDemuxer::isSameProfile(const std::shared_ptr<const StreamProfile> &lhs, const std::shared_ptr<const StreamProfile> &rhs) const {
    if(lhs == rhs) {
        return true;
    }
    if(!lhs || !rhs) {
        return false;
    }
    if(lhs->getFormat() != rhs->getFormat()) {
        return false;
    }
    if(lhs->is<VideoStreamProfile>() && rhs->is<VideoStreamProfile>()) {
        auto lhsVideo = lhs->as<VideoStreamProfile>();
        auto rhsVideo = rhs->as<VideoStreamProfile>();
        return lhsVideo->getWidth() == rhsVideo->getWidth() && lhsVideo->getHeight() == rhsVideo->getHeight() && lhsVideo->getFps() == rhsVideo->getFps();
    }
    return true;
}

void EGOStreamDemuxer::startStream(uint8_t camId, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(!isValidCamId(camId)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO stream cam id");
    }

    bool shouldStartRealPort = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(callbacks_.empty()) {
            activeProfile_      = profile;
            shouldStartRealPort = true;
        }
        else if(!isSameProfile(activeProfile_, profile)) {
            THROW_INVALID_PARAM_EXCEPTION("EGO shared endpoint requires identical width, height, fps and format for all active channels");
        }
        callbacks_[camId]    = std::move(callback);
        frameIndexes_[camId] = 1;  // Frame number starts from 1 for each virtual stream.
    }

    if(shouldStartRealPort) {
        try {
            realPort_->startStream(profile, [this](std::shared_ptr<Frame> frame) { onFrame(std::move(frame)); });
        }
        catch(...) {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_.erase(camId);
            if(callbacks_.empty()) {
                activeProfile_.reset();
            }
            throw;
        }
    }
}

void EGOStreamDemuxer::stopStream(uint8_t camId) {
    bool shouldStopRealPort = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(camId);
        if(callbacks_.empty() && activeProfile_) {
            shouldStopRealPort = true;
            activeProfile_.reset();
        }
    }

    if(shouldStopRealPort) {
        realPort_->stopAllStream();
        sendStopStreamVendorCmd();
    }
}

void EGOStreamDemuxer::stopAllStream() {
    bool shouldStopRealPort = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shouldStopRealPort = !callbacks_.empty() || activeProfile_;
        callbacks_.clear();
        activeProfile_.reset();
    }
    if(shouldStopRealPort) {
        realPort_->stopAllStream();
        sendStopStreamVendorCmd();
    }
}

void EGOStreamDemuxer::sendStopStreamVendorCmd() {
    if(config_.stopStreamPropertyId < 0) {
        return;
    }
    // Match VideoSensor::stop() ordering: the UVC endpoint has already been closed above, now tell the
    // firmware to stop the shared pipeline. All channels of a group share one UVC interface, so this is
    // sent once (last channel stopped) on the real port, not per virtual sensor.
    //
    // EGODepth exposes a single vendor XU (on the Color-group interface); the IR group therefore supplies
    // that interface as vendorCmdPort so its stop command reaches the XU. When unset the stop command
    // rides the group's own data port (EgoPro / Color-group case, where the XU shares that interface).
    std::shared_ptr<ISourcePort> cmdPort = config_.vendorCmdPort ? config_.vendorCmdPort : std::static_pointer_cast<ISourcePort>(realPort_);
    auto                         uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(cmdPort);
    if(!uvcPort) {
        return;
    }
    // EGO firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
    // so that Linux/libuvc (which routes by unit number) targets the correct unit.
    uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
    auto            accessor = std::make_shared<VendorPropertyAccessor>(owner_, cmdPort);
    OBPropertyValue value;
    value.intValue = 1;
    TRY_EXECUTE({ accessor->setPropertyValue(config_.stopStreamPropertyId, value); })
}

void EGOStreamDemuxer::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame || frame->getMetadataSize() < sizeof(StandardUvcFramePayloadHeader)) {
        LOG_WARN_INTVL("EGO shared stream frame metadata is missing, drop frame");
        return;
    }

    const auto          *metadata = reinterpret_cast<const StandardUvcFramePayloadHeader *>(frame->getMetadata());
    const auto           camId    = getEgoCamId(metadata);
    MutableFrameCallback callback;
    uint64_t             frameIndex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        iter = callbacks_.find(camId);
        if(iter == callbacks_.end()) {
            LOG_WARN_INTVL("EGO shared stream frame has inactive or invalid channel id {}, drop frame", camId);
            return;
        }
        callback   = iter->second;
        frameIndex = frameIndexes_[camId]++;
    }

    const auto expectedFrameType = mapCamIdToFrameType(camId);
    if(expectedFrameType != OB_FRAME_UNKNOWN && frame->getType() != expectedFrameType) {
        auto streamType = utils::mapFrameTypeToStreamType(expectedFrameType);
        auto profile    = frame->getStreamProfile();
        auto format     = frame->getFormat();
        auto width      = 0u;
        auto height     = 0u;
        auto fps        = 0u;
        if(profile && profile->is<const VideoStreamProfile>()) {
            auto videoProfile = profile->as<const VideoStreamProfile>();
            width             = videoProfile->getWidth();
            height            = videoProfile->getHeight();
            fps               = videoProfile->getFps();
        }
        auto remappedProfile = StreamProfileFactory::createVideoStreamProfile(streamType, format, width, height, fps);
        auto remappedFrame   = FrameFactory::createFrameFromStreamProfile(remappedProfile);
        remappedFrame->updateData(frame->getData(), frame->getDataSize());
        remappedFrame->copyInfoFromOther(frame);
        frame = std::move(remappedFrame);
    }

    frame->setNumber(frameIndex);
    callback(frame);
}

}  // namespace libobsensor

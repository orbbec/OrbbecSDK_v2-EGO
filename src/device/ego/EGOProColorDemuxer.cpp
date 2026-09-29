// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOProColorDemuxer.hpp"

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
constexpr uint8_t EGOPRO_MAX_COLOR_ID                = 3;
constexpr size_t  EGOPRO_SCR_SOURCE_CLOCK_LOW_OFFSET = 4;

uint8_t getEgoProColorId(const StandardUvcFramePayloadHeader *metadata) {
    const auto scrSof = static_cast<uint16_t>(metadata->scrSourceClock[EGOPRO_SCR_SOURCE_CLOCK_LOW_OFFSET])
                        | (static_cast<uint16_t>(metadata->scrSourceClock[EGOPRO_SCR_SOURCE_CLOCK_LOW_OFFSET + 1]) << 8);
    return static_cast<uint8_t>((scrSof >> 11) & 0x07);
}

OBFrameType mapColorIdToFrameType(uint8_t colorId) {
    switch(colorId) {
    case 0:
        return OB_FRAME_COLOR_LEFT;
    case 1:
        return OB_FRAME_COLOR_RIGHT;
    case 2:
        return OB_FRAME_COLOR_SIDE_RIGHT;
    case 3:
        return OB_FRAME_COLOR_SIDE_LEFT;
    default:
        return OB_FRAME_UNKNOWN;
    }
}

class EGOProColorVirtualPort : public IVideoStreamPort {
public:
    EGOProColorVirtualPort(std::shared_ptr<EGOProColorDemuxer> demuxer, uint8_t colorId) : demuxer_(std::move(demuxer)), colorId_(colorId) {}

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return demuxer_->getSourcePortInfo();
    }

    StreamProfileList getStreamProfileList() override {
        return demuxer_->getStreamProfileList();
    }

    void startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override {
        demuxer_->startStream(colorId_, std::move(profile), std::move(callback));
    }

    void stopStream(std::shared_ptr<const StreamProfile> profile) override {
        (void)profile;
        demuxer_->stopStream(colorId_);
    }

    void stopAllStream() override {
        demuxer_->stopStream(colorId_);
    }

private:
    std::shared_ptr<EGOProColorDemuxer> demuxer_;
    uint8_t                             colorId_;
};
}  // namespace

EGOProColorDemuxer::EGOProColorDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort) : owner_(owner), realPort_(std::move(realPort)) {
    if(!realPort_) {
        THROW_INVALID_PARAM_EXCEPTION("EgoPro color demuxer requires a real video stream port");
    }
}

EGOProColorDemuxer::~EGOProColorDemuxer() noexcept {
    TRY_EXECUTE(stopAllStream());
}

IDevice *EGOProColorDemuxer::getOwner() const {
    return owner_;
}

std::shared_ptr<IVideoStreamPort> EGOProColorDemuxer::createVirtualPort(uint8_t colorId) {
    if(colorId > EGOPRO_MAX_COLOR_ID) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EgoPro color id");
    }
    return std::make_shared<EGOProColorVirtualPort>(shared_from_this(), colorId);
}

StreamProfileList EGOProColorDemuxer::getStreamProfileList() {
    return realPort_->getStreamProfileList();
}

std::shared_ptr<const SourcePortInfo> EGOProColorDemuxer::getSourcePortInfo() const {
    return realPort_->getSourcePortInfo();
}

bool EGOProColorDemuxer::isSameProfile(const std::shared_ptr<const StreamProfile> &lhs, const std::shared_ptr<const StreamProfile> &rhs) const {
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

void EGOProColorDemuxer::startStream(uint8_t colorId, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(colorId > EGOPRO_MAX_COLOR_ID) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EgoPro color id");
    }

    bool shouldStartRealPort = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(callbacks_.empty()) {
            activeProfile_      = profile;
            shouldStartRealPort = true;
        }
        else if(!isSameProfile(activeProfile_, profile)) {
            THROW_INVALID_PARAM_EXCEPTION("EgoPro shared color endpoint requires identical width, height, fps and format for all active channels");
        }
        callbacks_[colorId]       = std::move(callback);
        frameIndexes_[colorId]    = 1;  // Frame number starts from 1 for each virtual stream.
    }

    if(shouldStartRealPort) {
        try {
            realPort_->startStream(profile, [this](std::shared_ptr<Frame> frame) { onFrame(std::move(frame)); });
        }
        catch(...) {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_.erase(colorId);
            if(callbacks_.empty()) {
                activeProfile_.reset();
            }
            throw;
        }
    }
}

void EGOProColorDemuxer::stopStream(uint8_t colorId) {
    bool shouldStopRealPort = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(colorId);
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

void EGOProColorDemuxer::stopAllStream() {
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

void EGOProColorDemuxer::sendStopStreamVendorCmd() {
    // Match VideoSensor::stop() ordering: the UVC endpoint has already been closed above, now tell the
    // firmware to stop the shared color pipeline. On EgoPro all 4 channels share one UVC interface, so
    // this is sent once (last channel stopped) on the real port, not per virtual color sensor.
    auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(realPort_);
    if(!uvcPort) {
        return;
    }
    // EGO firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
    // so that Linux/libuvc (which routes by unit number) targets the correct unit.
    uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
    auto            accessor = std::make_shared<VendorPropertyAccessor>(owner_, realPort_);
    OBPropertyValue value;
    value.intValue = 1;
    TRY_EXECUTE({ accessor->setPropertyValue(OB_PROP_STOP_COLOR_STREAM_BOOL, value); })
}

void EGOProColorDemuxer::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame || frame->getMetadataSize() < sizeof(StandardUvcFramePayloadHeader)) {
        LOG_WARN_INTVL("EgoPro color frame metadata is missing, drop frame");
        return;
    }

    const auto          *metadata = reinterpret_cast<const StandardUvcFramePayloadHeader *>(frame->getMetadata());
    const auto           colorId  = getEgoProColorId(metadata);
    MutableFrameCallback callback;
    uint64_t                             frameIndex;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        iter = callbacks_.find(colorId);
        if(iter == callbacks_.end()) {
            LOG_WARN_INTVL("EgoPro color frame has inactive or invalid channel id {}, drop frame", colorId);
            return;
        }
        callback   = iter->second;
        frameIndex = frameIndexes_[colorId]++;
    }

    const auto expectedFrameType = mapColorIdToFrameType(colorId);
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

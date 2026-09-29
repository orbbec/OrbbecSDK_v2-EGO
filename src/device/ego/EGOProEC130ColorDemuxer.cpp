// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOProEC130ColorDemuxer.hpp"

#include "exception/ObException.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "logger/LoggerInterval.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "usb/uvc/UvcDevicePort.hpp"
#include "usb/uvc/UvcTypes.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "component/property/InternalProperty.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "InternalTypes.hpp"

#include <set>

namespace libobsensor {
namespace {
constexpr uint8_t kMaxColorId = 3;

uint8_t colorId(const StandardUvcFramePayloadHeader *metadata) {
    auto low = static_cast<uint16_t>(metadata->scrSourceClock[4]) | (static_cast<uint16_t>(metadata->scrSourceClock[5]) << 8);
    return static_cast<uint8_t>((low >> 11) & 0x07);
}

OBFrameType frameType(uint8_t id) {
    switch(id) {
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

class VirtualPort : public IVideoStreamPort {
public:
    VirtualPort(std::shared_ptr<EGOProEC130ColorDemuxer> demuxer, uint8_t id) : demuxer_(std::move(demuxer)), id_(id) {}
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override {
        return demuxer_->getSourcePortInfo();
    }
    StreamProfileList getStreamProfileList() override {
        return demuxer_->getStreamProfileList(id_);
    }
    void startStream(std::shared_ptr<const StreamProfile> p, MutableFrameCallback cb) override {
        demuxer_->startStream(id_, std::move(p), std::move(cb));
    }
    void stopStream(std::shared_ptr<const StreamProfile>) override {
        demuxer_->stopStream(id_);
    }
    void stopAllStream() override {
        demuxer_->stopStream(id_);
    }

private:
    std::shared_ptr<EGOProEC130ColorDemuxer> demuxer_;
    uint8_t                                  id_;
};
}  // namespace

EGOProEC130ColorDemuxer::EGOProEC130ColorDemuxer(IDevice *owner, std::shared_ptr<IVideoStreamPort> realPort) : owner_(owner), realPort_(std::move(realPort)) {
    if(!realPort_)
        THROW_INVALID_PARAM_EXCEPTION("EgoPro EC130 color demuxer requires a real video stream port");
}

EGOProEC130ColorDemuxer::~EGOProEC130ColorDemuxer() noexcept {
    TRY_EXECUTE(stopAllStream());
}
IDevice *EGOProEC130ColorDemuxer::getOwner() const {
    return owner_;
}

std::shared_ptr<IVideoStreamPort> EGOProEC130ColorDemuxer::createVirtualPort(uint8_t id) {
    if(id > kMaxColorId)
        THROW_INVALID_PARAM_EXCEPTION("Invalid EgoPro EC130 color id");
    return std::make_shared<VirtualPort>(shared_from_this(), id);
}

StreamProfileList EGOProEC130ColorDemuxer::getStreamProfileList(uint8_t id) {
    auto transports = realPort_->getStreamProfileList();
    if(id > kMaxColorId)
        THROW_INVALID_PARAM_EXCEPTION("Invalid EgoPro EC130 color id");
    std::vector<OBEffectiveStreamProfile> caps;
    // The template comma is wrapped in extra parentheses so BEGIN_TRY_EXECUTE does not parse it as a second macro argument.
    BEGIN_TRY_EXECUTE({
        caps = (owner_->getPropertyServer()->getStructureDataListProtoV1_1_T<OBEffectiveStreamProfile, 0>(OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST));
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_WARN("EC130 camera capability list unavailable, falling back to UVC profiles.");
        return transports;
    })
    if(caps.empty())
        return transports;
    auto type           = utils::mapFrameTypeToStreamType(frameType(id));
    auto sensor         = utils::mapStreamTypeToSensorType(type);
    bool explicitCamera = false;
    for(const auto &entry: caps)
        explicitCamera = explicitCamera || entry.sensorType == sensor;
    StreamProfileList result;
    for(const auto &entry: caps) {
        if(entry.sensorType != (explicitCamera ? sensor : OB_SENSOR_COLOR) || !entry.width || !entry.height || !entry.maxFps)
            continue;
        std::set<uint32_t> rates{ entry.maxFps };
        for(const auto &transport: transports)
            if(transport->is<VideoStreamProfile>()) {
                auto fps = transport->as<VideoStreamProfile>()->getFps();
                if(fps && fps <= entry.maxFps)
                    rates.insert(fps);
            }
        auto format = entry.format == OB_FORMAT_HEVC ? OB_FORMAT_H265 : entry.format;
        for(auto fps: rates) {
            auto p         = StreamProfileFactory::createVideoStreamProfile(type, format, entry.width, entry.height, fps);
            bool duplicate = false;
            for(const auto &old: result)
                duplicate = duplicate || isSameProfile(old, p);
            if(!duplicate)
                result.push_back(p);
        }
    }
    return result;
}

std::shared_ptr<const SourcePortInfo> EGOProEC130ColorDemuxer::getSourcePortInfo() const {
    return realPort_->getSourcePortInfo();
}

bool EGOProEC130ColorDemuxer::isSameProfile(const std::shared_ptr<const StreamProfile> &a, const std::shared_ptr<const StreamProfile> &b) const {
    if(a == b)
        return true;
    if(!a || !b || a->getFormat() != b->getFormat())
        return false;
    if(!a->is<VideoStreamProfile>() || !b->is<VideoStreamProfile>())
        return true;
    auto av = a->as<VideoStreamProfile>();
    auto bv = b->as<VideoStreamProfile>();
    return av->getWidth() == bv->getWidth() && av->getHeight() == bv->getHeight() && av->getFps() == bv->getFps();
}

void EGOProEC130ColorDemuxer::startStream(uint8_t id, std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) {
    if(id > kMaxColorId || !profile || !profile->is<VideoStreamProfile>() || !callback)
        THROW_INVALID_PARAM_EXCEPTION("Invalid EC130 color stream request");
    auto     requested        = profile->as<VideoStreamProfile>();
    auto     transportProfile = std::shared_ptr<const StreamProfile>();
    uint32_t largestArea      = 0;
    bool     fpsMatched       = false;
    auto     transportProfiles = realPort_->getStreamProfileList();
    auto     isSameVideoFormat = [](OBFormat a, OBFormat b) {
        if(a == b)
            return true;
        return (a == OB_FORMAT_H265 || a == OB_FORMAT_HEVC) && (b == OB_FORMAT_H265 || b == OB_FORMAT_HEVC);
    };

    for(const auto &candidate: transportProfiles) {
        if(!candidate->is<VideoStreamProfile>()) {
            LOG_INFO("EC130 UVC candidate: non-video profile");
            continue;
        }
        auto video       = candidate->as<VideoStreamProfile>();
        auto area        = video->getWidth() * video->getHeight();
        bool match       = video->getFps() == requested->getFps();
        bool formatMatch = isSameVideoFormat(candidate->getFormat(), profile->getFormat());
        if(!formatMatch)
            continue;
        if(area > largestArea || (area == largestArea && match && !fpsMatched)) {
            transportProfile = candidate;
            largestArea      = area;
            fpsMatched       = match;
        }
    }

    if(!transportProfile || requested->getWidth() * requested->getHeight() > largestArea)
        THROW_INVALID_PARAM_EXCEPTION("EC130 resolution exceeds UVC transport capacity");
    bool startReal = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(callbacks_.empty()) {
            activeProfile_ = transportProfile;
            startReal      = true;
        }
        callbacks_[id]       = std::move(callback);
        channelProfiles_[id] = profile;
        frameIndexes_[id]    = 1;
    }
    if(startReal) {
        try {
            realPort_->startStream(transportProfile, [this](std::shared_ptr<Frame> f) { onFrame(std::move(f)); });
        }
        catch(...) {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacks_.erase(id);
            channelProfiles_.erase(id);
            activeProfile_.reset();
            throw;
        }
    }
}

void EGOProEC130ColorDemuxer::stopStream(uint8_t id) {
    bool stop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(id);
        channelProfiles_.erase(id);
        if(callbacks_.empty() && activeProfile_) {
            activeProfile_.reset();
            stop = true;
        }
    }
    if(stop) {
        realPort_->stopAllStream();
        sendStopStreamVendorCmd();
    }
}
void EGOProEC130ColorDemuxer::stopAllStream() {
    bool stop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop = !callbacks_.empty() || activeProfile_;
        callbacks_.clear();
        channelProfiles_.clear();
        activeProfile_.reset();
    }
    if(stop) {
        realPort_->stopAllStream();
        sendStopStreamVendorCmd();
    }
}
void EGOProEC130ColorDemuxer::sendStopStreamVendorCmd() {
    auto uvc = std::dynamic_pointer_cast<UvcDevicePort>(realPort_);
    if(!uvc)
        return;
    uvc->updateXuUnit(OB_EGO_XU_UNIT);
    auto            accessor = std::make_shared<VendorPropertyAccessor>(owner_, realPort_);
    OBPropertyValue value;
    value.intValue = 1;
    TRY_EXECUTE({ accessor->setPropertyValue(OB_PROP_STOP_COLOR_STREAM_BOOL, value); })
}

void EGOProEC130ColorDemuxer::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame || frame->getMetadataSize() < sizeof(StandardUvcFramePayloadHeader))
        return;
    auto                                 id = colorId(reinterpret_cast<const StandardUvcFramePayloadHeader *>(frame->getMetadata()));
    MutableFrameCallback                 callback;
    std::shared_ptr<const StreamProfile> profile;
    uint64_t                             index;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto                        it = callbacks_.find(id);
        if(it == callbacks_.end())
            return;
        callback = it->second;
        profile  = channelProfiles_.at(id);
        index    = frameIndexes_[id]++;
    }
    auto expected = frameType(id);
    if(expected != OB_FRAME_UNKNOWN) {
        auto video    = profile->as<VideoStreamProfile>();
        auto remapped = StreamProfileFactory::createVideoStreamProfile(utils::mapFrameTypeToStreamType(expected), profile->getFormat(), video->getWidth(),
                                                                       video->getHeight(), video->getFps());
        if(frame->getDataSize() > remapped->as<VideoStreamProfile>()->getMaxFrameDataSize())
            return;
        auto output = FrameFactory::createFrameFromStreamProfile(remapped);
        output->updateData(frame->getData(), frame->getDataSize());
        output->Frame::copyInfoFromOther(frame);
        frame = std::move(output);
    }
    frame->setNumber(index);
    callback(frame);
}
}  // namespace libobsensor

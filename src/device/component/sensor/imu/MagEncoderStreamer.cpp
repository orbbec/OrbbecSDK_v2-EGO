// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MagEncoderStreamer.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "hid/HidTypes.hpp"
#include "logger/LoggerInterval.hpp"

namespace libobsensor {

// One mechanical revolution maps to a raw range of [0, 32768); scale raw counts to degrees.
static const float MAG_ENCODER_RAW_TO_DEGREE = 360.0f / 32768.0f;

MagEncoderStreamer::MagEncoderStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend)
    : owner_(owner), backend_(backend), running_(false), frameIndex_(0) {
    LOG_DEBUG("MagEncoderStreamer created");
}

MagEncoderStreamer::~MagEncoderStreamer() noexcept {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }

    if(running_) {
        TRY_EXECUTE(backend_->stopStream());
        running_ = false;
    }
}

void MagEncoderStreamer::startStream(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_[sp] = callback;
        if(running_) {
            return;
        }
    }
    frameIndex_ = 1;  // frame number start from 1

    backend_->startStream([this](std::shared_ptr<Frame> frame) { MagEncoderStreamer::parseMagEncoderData(frame); });
    running_ = true;
}

void MagEncoderStreamer::stopStream(std::shared_ptr<const StreamProfile> sp) {
    LOG_DEBUG("MagEncoderStreamer stop....");
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        auto                        iter = callbacks_.find(sp);
        if(iter == callbacks_.end()) {
            THROW_ITEM_NOT_FOUND_EXCEPTION("Stop stream failed, stream profile not found.");
        }

        callbacks_.erase(iter);
        if(!callbacks_.empty()) {
            return;
        }
    }
    backend_->stopStream();
    running_    = false;
    frameIndex_ = 1;
    LOG_DEBUG("MagEncoderStreamer stop finished.");
}

void MagEncoderStreamer::parseMagEncoderData(std::shared_ptr<Frame> frame) {
    auto data     = frame->getData();
    auto dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    const auto *header = reinterpret_cast<const OBHidHeader *>(data);
    if(header->reportId != OB_HID_REPORT_ID_MAG_ENCODER || header->groupCount == 0 || header->groupLen < sizeof(Mt6826sAngleReport)) {
        return;
    }

    const auto expectedSize = sizeof(OBHidHeader) + static_cast<size_t>(header->groupLen) * header->groupCount;
    if(dataSize < expectedSize) {
        LOG_WARN_INTVL("Mag encoder packet is incomplete, drop packet! dataSize={}, expectedSize={}, groupLen={}, groupCount={}", dataSize, expectedSize,
                       header->groupLen, header->groupCount);
        return;
    }

    std::shared_ptr<const StreamProfile> streamProfile;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        for(const auto &iter: callbacks_) {
            if(iter.first->getType() == OB_STREAM_MAG_ENCODER) {
                streamProfile = iter.first;
                break;
            }
        }
    }
    if(!streamProfile) {
        return;
    }

    MutableFrameCallback callback;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        auto                        iter = callbacks_.find(streamProfile);
        if(iter == callbacks_.end()) {
            return;
        }
        callback = iter->second;
    }

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header->groupCount; ++groupIndex) {
        const auto *report = reinterpret_cast<const Mt6826sAngleReport *>(groupBase + static_cast<size_t>(header->groupLen) * groupIndex);

        auto magEncoderFrame   = FrameFactory::createFrameFromStreamProfile(streamProfile);
        auto frameData         = reinterpret_cast<MagEncoderFrame::Data *>(magEncoderFrame->getDataMutable());
        frameData->angleDegree = static_cast<float>(report->angle) /100;
        frameData->status      = report->status;
        frameData->timestampUs = report->timestamp;

        magEncoderFrame->setNumber(frameIndex_++);
        magEncoderFrame->setTimeStampUsec(report->timestamp);
        magEncoderFrame->setSystemTimeStampUsec(frame->getSystemTimeStampUsec());
        magEncoderFrame->setSteadyTimeStampUsec(frame->getSteadyTimeStampUsec());
        callback(magEncoderFrame);
    }
}

IDevice *MagEncoderStreamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor

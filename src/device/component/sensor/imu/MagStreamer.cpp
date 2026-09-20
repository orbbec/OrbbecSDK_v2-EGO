// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MagStreamer.hpp"

#include "ImuPayloadDecoder.hpp"
#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "hid/HidTypes.hpp"
#include "logger/LoggerInterval.hpp"

namespace libobsensor {

// 1 Gauss = 100 uT. The int16 magX/Y/Z registers span [-32768, 32767] over the configured
// full-scale range (expressed in Gauss), so uT = raw / 32768 * fullScaleGauss * 100.
static float magFullScaleToGauss(OBMagFullScaleRange fs) {
    switch(fs) {
    case OB_MAG_FS_4G:
        return 4.0f;
    case OB_MAG_FS_8G:
        return 8.0f;
    case OB_MAG_FS_12G:
        return 12.0f;
    case OB_MAG_FS_16G:
        return 16.0f;
    default:
        return 4.0f;
    }
}

MagStreamer::MagStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend) : owner_(owner), backend_(backend), running_(false), frameIndex_(0) {
    LOG_DEBUG("MagStreamer created");
}

MagStreamer::~MagStreamer() noexcept {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }

    if(running_) {
        TRY_EXECUTE(backend_->stopStream());
        running_ = false;
    }
}

void MagStreamer::startStream(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_[sp] = callback;
        if(running_) {
            return;
        }
    }
    frameIndex_ = 1;  // frame number start from 1

    backend_->startStream([this](std::shared_ptr<Frame> frame) { MagStreamer::parseMagData(frame); });
    running_ = true;
}

void MagStreamer::stopStream(std::shared_ptr<const StreamProfile> sp) {
    LOG_DEBUG("MagStreamer stop....");
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
    LOG_DEBUG("MagStreamer stop finished.");
}

void MagStreamer::parseMagData(std::shared_ptr<Frame> frame) {
    auto data     = frame->getData();
    auto dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    const auto *header = reinterpret_cast<const OBHidHeader *>(data);
    if(header->reportId != OB_HID_REPORT_ID_MAGNETOMETER || header->groupCount == 0 || header->groupLen < sizeof(OBMagnetometerData)) {
        return;
    }

    const auto expectedSize = sizeof(OBHidHeader) + static_cast<size_t>(header->groupLen) * header->groupCount;
    if(dataSize < expectedSize) {
        LOG_WARN_INTVL("Magnetometer packet is incomplete, drop packet! dataSize={}, expectedSize={}, groupLen={}, groupCount={}", dataSize, expectedSize,
                       header->groupLen, header->groupCount);
        return;
    }

    std::shared_ptr<const MagStreamProfile> magStreamProfile;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        for(const auto &iter: callbacks_) {
            if(iter.first->is<libobsensor::MagStreamProfile>()) {
                magStreamProfile = iter.first->as<libobsensor::MagStreamProfile>();
                break;
            }
        }
    }
    if(!magStreamProfile) {
        return;
    }

    MutableFrameCallback callback;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        auto                        iter = callbacks_.find(magStreamProfile);
        if(iter == callbacks_.end()) {
            return;
        }
        callback = iter->second;
    }

    // uT per raw count for the configured full-scale range.
    const float rawToMicroTesla = magFullScaleToGauss(magStreamProfile->getFullScaleRange()) * 100.0f / 32768.0f;

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header->groupCount; ++groupIndex) {
        const auto *report = reinterpret_cast<const OBMagnetometerData *>(groupBase + static_cast<size_t>(header->groupLen) * groupIndex);

        uint64_t timestamp = ((uint64_t)report->timestamp[0] | ((uint64_t)report->timestamp[1] << 32));

        auto magFrame         = FrameFactory::createFrameFromStreamProfile(magStreamProfile);
        auto magFrameData     = reinterpret_cast<MagFrame::Data *>(magFrame->getDataMutable());
        magFrameData->value.x = static_cast<float>(report->magX) * rawToMicroTesla;
        magFrameData->value.y = static_cast<float>(report->magY) * rawToMicroTesla;
        magFrameData->value.z = static_cast<float>(report->magZ) * rawToMicroTesla;
        magFrameData->temp    = 0.0f;

        magFrame->setNumber(frameIndex_++);
        magFrame->setTimeStampUsec(timestamp);
        magFrame->setSystemTimeStampUsec(frame->getSystemTimeStampUsec());
        magFrame->setSteadyTimeStampUsec(frame->getSteadyTimeStampUsec());
        callback(magFrame);
    }
}

IDevice *MagStreamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor

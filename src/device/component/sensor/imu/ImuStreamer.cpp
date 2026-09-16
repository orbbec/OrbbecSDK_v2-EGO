// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ImuStreamer.hpp"

#include "frame/Frame.hpp"
#include "frame/FrameFactory.hpp"
#include "stream/StreamProfile.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "publicfilters/IMUCorrector.hpp"

namespace libobsensor {

const size_t IMU_FILTER_FRAME_QUEUE_SIZE = 100;

ImuStreamer::ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, const std::shared_ptr<IFilter> &filter,
                         std::shared_ptr<IImuCalculator> imuCalculator)
    : ImuStreamer(owner, backend, std::vector<std::shared_ptr<IFilter>>({ filter }), imuCalculator) {}

ImuStreamer::ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, std::vector<std::shared_ptr<IFilter>> filters,
                         std::shared_ptr<IImuCalculator> imuCalculator)
    : owner_(owner), backend_(backend), filters_(std::move(filters)), running_(false), frameIndex_(0) {

    if(!imuCalculator) {
        // default to ICM42668P
        imuCalculator = std::make_shared<ImuCalculatorICM42668P>();
    }
    // Wrap the legacy calculator in the calculator-based decoder to keep existing devices' behavior unchanged.
    decoder_ = std::make_shared<LegacyImuCalculatorDecoder>(imuCalculator);

    initFilters();

    LOG_DEBUG("ImuStreamer created");
}

ImuStreamer::ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, const std::shared_ptr<IFilter> &filter,
                         std::shared_ptr<IImuPayloadDecoder> decoder)
    : ImuStreamer(owner, backend, std::vector<std::shared_ptr<IFilter>>({ filter }), decoder) {}

ImuStreamer::ImuStreamer(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend, std::vector<std::shared_ptr<IFilter>> filters,
                         std::shared_ptr<IImuPayloadDecoder> decoder)
    : owner_(owner), backend_(backend), filters_(std::move(filters)), decoder_(std::move(decoder)), running_(false), frameIndex_(0) {

    if(!decoder_) {
        // default to legacy int16 register format with ICM42668P calculator
        decoder_ = std::make_shared<LegacyImuCalculatorDecoder>(std::make_shared<ImuCalculatorICM42668P>());
    }

    initFilters();

    LOG_DEBUG("ImuStreamer created");
}

void ImuStreamer::initFilters() {
    auto iter = filters_.begin();
    while(iter != filters_.end()) {
        (*iter)->resizeFrameQueue(IMU_FILTER_FRAME_QUEUE_SIZE);
        auto nextIter = iter + 1;
        if(nextIter == filters_.end()) {
            (*iter)->setCallback([this](std::shared_ptr<Frame> frame) { outputFrame(frame); });
        }
        else {
            (*iter)->setCallback([nextIter](std::shared_ptr<Frame> frame) { (*nextIter)->pushFrame(frame); });
        }

        iter++;
    }
}

ImuStreamer::~ImuStreamer() noexcept {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_.clear();
    }

    if(running_) {
        TRY_EXECUTE(backend_->stopStream());
        for(auto &filter: filters_) {
            filter->reset();
        }
        running_ = false;
    }
}

void ImuStreamer::startStream(std::shared_ptr<const StreamProfile> sp, MutableFrameCallback callback) {
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks_[sp] = callback;
        if(running_) {
            return;
        }
    }
    frameIndex_ = 1;  // frame number start from 1

    // Some devices report incorrect timestamps in the first few IMU frames.
    // Although the actual number of affected frames may vary, discarding
    // the first 8 frames provides a consistent safeguard against invalid timing data
    ignoreLeadingFrameCount_ = 8;

    int retry = 3;
    do {
        try {
            backend_->startStream([this](std::shared_ptr<Frame> frame) { ImuStreamer::parseIMUData(frame); });
            break;
        }
        catch(const libobsensor::libobsensor_exception &e) {  // Only retry when socket is not ready
            LOG_ERROR("ImuStreamer start failed");
            std::string msg = e.getMessage();
            // TODO:
            if(msg.find("socket is not ready & timeout") != std::string::npos && --retry > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            throw;
        }
    } while(retry > 0);

    running_ = true;
}

void ImuStreamer::stopStream(std::shared_ptr<const StreamProfile> sp) {
    LOG_DEBUG("ImuStreamer stop....");
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
    LOG_DEBUG("ImuStreamer stop backend....");
    backend_->stopStream();

    LOG_DEBUG("ImuStreamer reset filters....");
    for(auto &filter: filters_) {
        filter->reset();
    }
    running_    = false;
    frameIndex_ = 1;  // reset frame number
    LOG_DEBUG("ImuStreamer stop finished.");
}

void ImuStreamer::parseIMUData(std::shared_ptr<Frame> frame) {
    auto data     = frame->getData();
    auto dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    OBHidHeader *header = (OBHidHeader *)data;

    if(header->reportId != OB_HID_REPORT_ID_IMU) {
        return;
    }

    // Strictly validate the group length reported by firmware against what the injected decoder expects.
    const size_t groupSize = decoder_->groupSize();
    if(header->groupLen != groupSize) {
        LOG_WARN_INTVL("Imu header is invalid, drop imu package!, unexpected groupLen. groupLen={}, expected={}, groupCount={}, dataSize={}", header->groupLen,
                       groupSize, header->groupCount, dataSize);
        return;
    }

    const auto computeDataSize = sizeof(OBHidHeader) + groupSize * header->groupCount;
    if(dataSize < computeDataSize) {
        LOG_WARN_INTVL("Imu header is invalid, drop imu package!, invalid data size. dataSize={}, computeDataSize={}, groupCount={}", dataSize, computeDataSize,
                       header->groupCount);
        return;
    }

    int32_t discardCount = 0;
    if(ignoreLeadingFrameCount_ > 0) {
        discardCount = ignoreLeadingFrameCount_;
        if(ignoreLeadingFrameCount_ > header->groupCount) {
            discardCount = header->groupCount;
        }
        //  Discard the first few frames provides a consistent safeguard against invalid timing data
        LOG_DEBUG("Discard the first few IMU frames. Received count: {}, left count: {}, discard count: {}", header->groupCount, ignoreLeadingFrameCount_,
                  discardCount);
        ignoreLeadingFrameCount_ -= discardCount;
        if(discardCount >= header->groupCount) {
            // no any more data for this package
            return;
        }
    }

    std::shared_ptr<const AccelStreamProfile> accelStreamProfile;
    std::shared_ptr<const GyroStreamProfile>  gyroStreamProfile;
    std::shared_ptr<const MagStreamProfile>   magStreamProfile;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        for(const auto &iter: callbacks_) {
            if(iter.first->is<libobsensor::AccelStreamProfile>()) {
                accelStreamProfile = iter.first->as<libobsensor::AccelStreamProfile>();
            }
            else if(iter.first->is<libobsensor::GyroStreamProfile>()) {
                gyroStreamProfile = iter.first->as<libobsensor::GyroStreamProfile>();
            }
            else if(iter.first->is<libobsensor::MagStreamProfile>()) {
                magStreamProfile = iter.first->as<libobsensor::MagStreamProfile>();
            }
        }
    }

    uint8_t *imuOrgData = (uint8_t *)data + sizeof(OBHidHeader);
    for(int groupIndex = discardCount; groupIndex < header->groupCount; groupIndex++) {
        auto frameSet = FrameFactory::createFrameSet();

        const uint8_t   *group    = imuOrgData + static_cast<size_t>(groupIndex) * groupSize;
        uint8_t          accelFsr = accelStreamProfile ? static_cast<uint8_t>(accelStreamProfile->getFullScaleRange()) : uint8_t(0);
        uint8_t          gyroFsr  = gyroStreamProfile ? static_cast<uint8_t>(gyroStreamProfile->getFullScaleRange()) : uint8_t(0);
        DecodedImuSample sample   = decoder_->decode(group, accelFsr, gyroFsr);

        uint64_t timestamp   = ((uint64_t)sample.timestamp[0] | ((uint64_t)sample.timestamp[1] << 32));
        auto     sysTspUs    = frame->getSystemTimeStampUsec();
        auto     steadyTspUs = frame->getSteadyTimeStampUsec();
        auto     frameIndex  = frameIndex_++;

        if(accelStreamProfile) {
            auto accelFrame     = FrameFactory::createFrameFromStreamProfile(accelStreamProfile);
            auto accelFrameData = (AccelFrame::Data *)accelFrame->getData();

            accelFrameData->value.x = sample.accel[0];
            accelFrameData->value.y = sample.accel[1];
            accelFrameData->value.z = sample.accel[2];
            accelFrameData->temp    = sample.temperature;

            accelFrame->setNumber(frameIndex);
            accelFrame->setTimeStampUsec(timestamp);
            accelFrame->setSystemTimeStampUsec(sysTspUs);
            accelFrame->setSteadyTimeStampUsec(steadyTspUs);
            frameSet->pushFrame(accelFrame);
        }

        if(gyroStreamProfile) {
            auto gyroFrame         = FrameFactory::createFrameFromStreamProfile(gyroStreamProfile);
            auto gyroFrameData     = (GyroFrame::Data *)gyroFrame->getData();
            gyroFrameData->value.x = sample.gyro[0];
            gyroFrameData->value.y = sample.gyro[1];
            gyroFrameData->value.z = sample.gyro[2];
            gyroFrameData->temp    = sample.temperature;

            gyroFrame->setNumber(frameIndex);
            gyroFrame->setTimeStampUsec(timestamp);
            gyroFrame->setSystemTimeStampUsec(sysTspUs);
            gyroFrame->setSteadyTimeStampUsec(steadyTspUs);
            frameSet->pushFrame(gyroFrame);
        }

        if(magStreamProfile && sample.hasMag) {
            auto magFrame         = FrameFactory::createFrameFromStreamProfile(magStreamProfile);
            auto magFrameData     = (MagFrame::Data *)magFrame->getData();
            magFrameData->value.x = sample.mag[0];
            magFrameData->value.y = sample.mag[1];
            magFrameData->value.z = sample.mag[2];
            magFrameData->temp    = sample.temperature;

            magFrame->setNumber(frameIndex);
            magFrame->setTimeStampUsec(timestamp);
            magFrame->setSystemTimeStampUsec(sysTspUs);
            magFrame->setSteadyTimeStampUsec(steadyTspUs);
            frameSet->pushFrame(magFrame);
        }
        if(!filters_.empty()) {
            filters_.front()->pushFrame(frameSet);
        }
        else {
            outputFrame(frameSet);
        }
    }
}

void ImuStreamer::outputFrame(std::shared_ptr<Frame> frame) {
    if(!frame) {
        return;
    }
    std::map<std::shared_ptr<const StreamProfile>, MutableFrameCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(cbMtx_);
        callbacks = callbacks_;
    }

    for(auto &callback: callbacks) {
        std::shared_ptr<Frame> callbackFrame = frame;
        if(frame->is<FrameSet>()) {
            auto frameSet  = frame->as<FrameSet>();
            auto frameType = utils::mapStreamTypeToFrameType(callback.first->getType());
            callbackFrame  = frameSet->getFrameMutable(frameType);
            if(!callbackFrame) {
                continue;
            }
        }
        if(callbackFrame->getFormat() != callback.first->getFormat()) {
            continue;
        }
        callback.second(callbackFrame);
    }
}

IDevice *ImuStreamer::getOwner() const {
    return owner_;
}

}  // namespace libobsensor

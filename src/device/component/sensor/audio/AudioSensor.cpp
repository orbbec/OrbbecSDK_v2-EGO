// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "AudioSensor.hpp"
#include "ISensorStreamStrategy.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/Utils.hpp"
#include "stream/StreamProfile.hpp"
#include "frame/Frame.hpp"

namespace libobsensor {

AudioSensor::AudioSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend) : SensorBase(owner, OB_SENSOR_AUDIO, backend) {
    auto port = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    if(!port) {
        THROW_INVALID_PARAM_EXCEPTION("Backend is not a valid stream port for AudioSensor");
    }

    auto backendSpList = port->getStreamProfileList();
    setStreamProfileList(backendSpList);

    LOG_DEBUG("AudioSensor created!");
}

AudioSensor::~AudioSensor() noexcept {
    try {
        disableStreamRecovery();
        stop();
    }
    catch(const std::exception &e) {
        LOG_ERROR("Exception occurred while destroying AudioSensor: {}", e.what());
    }
    frameQueue_.reset();
    LOG_DEBUG("AudioSensor destroyed!");
}

void AudioSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    LOG_INFO("Try to start audio stream: {}", sp);

    validateDeviceState(sp);

    {
        auto owner    = getOwner();
        auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
        if(strategy) {
            strategy->validateStream(sp);
            strategy->markStreamActivated(sp);
        }
    }

    activatedStreamProfile_      = sp;
    frameCallback_               = callback;
    currentBackendStreamProfile_ = sp;
    updateStreamState(STREAM_STATE_STARTING);

    auto port = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
    BEGIN_TRY_EXECUTE({
        // Audio is a high-rate chunk stream; use a modest queue depth.
        frameQueue_ = std::make_shared<SpscFrameQueue<Frame>>(32u);
        frameQueue_->start([this](std::shared_ptr<Frame> frame) { onBackendFrameCallback(frame); });
        port->startStream(currentBackendStreamProfile_, [this](std::shared_ptr<Frame> frame) {
            bool dropped = false;
            auto res     = frameQueue_->enforceEnqueue(frame, dropped);
            if(!res || dropped) {
                droppedFrameStatus_.fetch_or(OB_SDK_STATUS_FRAME_QUEUE_OVERFLOW, std::memory_order_relaxed);
                LOG_INTVL(LOG_INTVL_OBJECT_TAG, 10000, spdlog::level::warn, "Audio frame dropped because the queue is full or stopping!");
            }
        });
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        {
            auto owner    = getOwner();
            auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
            if(strategy) {
                strategy->markStreamDeactivated(activatedStreamProfile_);
            }
        }
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        frameQueue_.reset();
        throw;
    })
}

void AudioSensor::onBackendFrameCallback(std::shared_ptr<Frame> frame) {
    if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
        return;
    }
    updateStreamState(STREAM_STATE_STREAMING);
    outputFrame(frame);
}

void AudioSensor::stop() {
    if(!isStreamActivated()) {
        return;
    }

    waitRecoveringFinished();

    try {
        auto owner    = getOwner();
        auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
        if(strategy) {
            strategy->markStreamDeactivated(activatedStreamProfile_);
        }
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to mark audio stream as deactivated: {}", e.what());
    }

    updateStreamState(STREAM_STATE_STOPPING);

    try {
        auto port = std::dynamic_pointer_cast<IVideoStreamPort>(backend_);
        port->stopStream(currentBackendStreamProfile_);
    }
    catch(const std::exception &e) {
        LOG_WARN("Failed to stop audio stream port: {}", e.what());
    }

    updateStreamState(STREAM_STATE_STOPPED);

    if(frameQueue_) {
        frameQueue_->stop();
    }

    frameQueue_.reset();
    activatedStreamProfile_.reset();
    frameCallback_ = nullptr;
}

}  // namespace libobsensor

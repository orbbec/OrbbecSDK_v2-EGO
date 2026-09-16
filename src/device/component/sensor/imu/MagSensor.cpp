// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MagSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

MagSensor::MagSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_MAG, backend), streamer_(streamer) {
    auto propServer = owner->getPropertyServer();

    // The magnetometer has no dedicated ODR/full-scale-range presets; it is reported alongside accel/gyro in the
    // same IMU group. Describe it with a single stream profile whose sample rate mirrors the accelerometer's when
    // available, otherwise leave it unknown.
    auto            lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_MAG);
    OBIMUSampleRate sampleRate = OB_SAMPLE_RATE_UNKNOWN;
    if(propServer->isPropertySupported(OB_PROP_ACCEL_ODR_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        sampleRate = propServer->getPropertyValueT<OBIMUSampleRate>(OB_PROP_ACCEL_ODR_INT);
    }

    auto profile = StreamProfileFactory::createMagStreamProfile(lazySensor, sampleRate);
    streamProfileList_.emplace_back(profile);
    updateDefaultStreamProfile(profile);

    LOG_DEBUG("MagSensor is created!");
}

MagSensor::~MagSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("MagSensor is destroyed");
}

void MagSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    // validate device state
    validateDeviceState(sp);

    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    // The magnetometer shares the IMU report with accel/gyro. Best-effort enable the IMU switch so the device
    // starts reporting; if accel/gyro are already streaming this is a harmless no-op.
    // TODO: revisit the enable mechanism once firmware provides a dedicated magnetometer switch.
    if(propServer->isPropertySupported(OB_PROP_ACCEL_SWITCH_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
        TRY_EXECUTE(propServer->setPropertyValueT(OB_PROP_ACCEL_SWITCH_BOOL, true));
    }

    BEGIN_TRY_EXECUTE({
        streamer_->startStream(sp, [this](std::shared_ptr<Frame> frame) {
            if(streamState_ != STREAM_STATE_STREAMING && streamState_ != STREAM_STATE_STARTING) {
                return;
            }
            updateStreamState(STREAM_STATE_STREAMING);

            outputFrame(frame);
        });
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        throw;
    })
}

void MagSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stopStream(activatedStreamProfile_);
    // Do not clear OB_PROP_ACCEL_SWITCH_BOOL here: accel/gyro may still be streaming. The shared backend is
    // torn down by the ImuStreamer once its last callback is removed.
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor

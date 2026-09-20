// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MagEncoderSensor.hpp"
#include "IDevice.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

#pragma pack(1)
typedef struct {
    uint32_t num;
    OBMagEncoderSampleRate odr[16];
} MagEncoderSampleRateList;
#pragma pack()

MagEncoderSensor::MagEncoderSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer)
    : SensorBase(owner, OB_SENSOR_MAG_ENCODER, backend), streamer_(streamer) {
    auto propServer = owner->getPropertyServer();

    // The magnetic angle encoder streams a self-contained angle report on its own report id. It has no
    // full-scale range, but exposes an ODR preset list, so build one profile per supported sample rate.
    bool presetsLoaded = false;
    if(propServer->isPropertySupported(OB_STRUCT_GET_MAG_ENCODER_PRESETS_ODR_LIST, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        // The property is registered but the firmware may still not answer it (older firmware). Keep the
        // sensor creatable in that case instead of letting the exception break the whole device bring-up.
        try {
            auto odrList = propServer->getStructureDataT<MagEncoderSampleRateList>(OB_STRUCT_GET_MAG_ENCODER_PRESETS_ODR_LIST);

            // Preset list as reported by firmware, logged at debug level to ease device bring-up.
            LOG_DEBUG("Mag encoder presets from firmware: odrCount={}", odrList.num);
            for(uint32_t i = 0; i < odrList.num; i++) {
                LOG_DEBUG("  mag encoder sampleRate[{}] = {}", i, static_cast<int>(odrList.odr[i]));
            }

            auto lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_MAG_ENCODER);
            for(uint32_t i = 0; i < odrList.num; i++) {
                auto odr = odrList.odr[i];
                if(odr == 0) {
                    continue;
                }
                auto profile = StreamProfileFactory::createMagEncoderStreamProfile(lazySensor, odr);
                streamProfileList_.emplace_back(profile);
            }

            if(!streamProfileList_.empty()) {
                LOG_DEBUG("Mag encoder built {} stream profiles from the firmware presets", streamProfileList_.size());

                if(propServer->isPropertySupported(OB_PROP_MAG_ENCODER_ODR_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                    auto currentSampleRate = propServer->getPropertyValueT<OBMagEncoderSampleRate>(OB_PROP_MAG_ENCODER_ODR_INT);
                    LOG_DEBUG("Mag encoder current sample rate from firmware: {}", static_cast<int>(currentSampleRate));
                    auto defaultProfile = StreamProfileFactory::createMagEncoderStreamProfile(lazySensor, currentSampleRate);
                    updateDefaultStreamProfile(defaultProfile);
                }
                else {
                    LOG_WARN("Mag encoder odr property is not readable, the default stream profile is not updated from firmware");
                }

                presetsLoaded = true;
            }
            else {
                LOG_WARN("No valid mag encoder stream profile was built from the firmware preset list, check the odr values reported by the device");
            }
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to read mag encoder presets from firmware: {}, falling back to a single stream profile", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to read mag encoder presets from firmware, falling back to a single stream profile");
        }
    }

    if(!presetsLoaded) {
        // Firmware without a usable preset list: describe the encoder with a single self-contained profile
        // whose sample rate is left unknown, so start() keeps the rate the device is already using.
        streamProfileList_.clear();
        auto lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_MAG_ENCODER);
        auto profile    = StreamProfileFactory::createMagEncoderStreamProfile(lazySensor);
        streamProfileList_.emplace_back(profile);
        updateDefaultStreamProfile(profile);
    }

    LOG_DEBUG("MagEncoderSensor is created!");
}

MagEncoderSensor::~MagEncoderSensor() noexcept {
    if(isStreamActivated()) {
        TRY_EXECUTE(stop());
    }
    LOG_DEBUG("MagEncoderSensor is destroyed");
}

void MagEncoderSensor::start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) {
    validateDeviceState(sp);

    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    // Best-effort, in order: apply the ODR carried by the profile, then enable the encoder. Both
    // properties are optional. New firmware may start the encoder stream by default, and older firmware
    // may not expose the ODR at all, so a failed write is only logged and never stops the already-running
    // reportId=5 stream from being read.
    BEGIN_TRY_EXECUTE({
        auto magEncoderSp = sp->as<MagEncoderStreamProfile>();
        if(magEncoderSp && magEncoderSp->getSampleRate() != 0
           && propServer->isPropertySupported(OB_PROP_MAG_ENCODER_ODR_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_ENCODER_ODR_INT, static_cast<int>(magEncoderSp->getSampleRate()));
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to set magnetic encoder ODR"); })

    BEGIN_TRY_EXECUTE({
        if(propServer->isPropertySupported(OB_PROP_MAG_ENCODER_SWITCH_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_ENCODER_SWITCH_BOOL, true);
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({ LOG_WARN("Failed to enable magnetic encoder switch"); })

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

void MagEncoderSensor::stop() {
    updateStreamState(STREAM_STATE_STOPPING);
    streamer_->stopStream(activatedStreamProfile_);
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    // Best effort: only clear the switch where the device exposes it, and never let a property failure
    // turn into a stop failure.
    TRY_EXECUTE({
        if(propServer->isPropertySupported(OB_PROP_MAG_ENCODER_SWITCH_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_ENCODER_SWITCH_BOOL, false);
        }
    })
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor

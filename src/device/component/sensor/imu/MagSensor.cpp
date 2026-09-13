// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "MagSensor.hpp"
#include "IDevice.hpp"
#include "ISensorStreamStrategy.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamProfileFactory.hpp"

namespace libobsensor {

#pragma pack(1)
typedef struct {
    uint32_t        num;
    OBMagSampleRate odr[16];
} MagSampleRateList;

typedef struct {
    uint32_t            num;
    OBMagFullScaleRange fs[16];
} MagFullScaleRangeList;
#pragma pack()

MagSensor::MagSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer,
                     bool enableStreamStrategyValidation)
    : SensorBase(owner, OB_SENSOR_MAG, backend), streamer_(streamer), enableStreamStrategyValidation_(enableStreamStrategyValidation) {
    auto propServer = owner->getPropertyServer();

    // The magnetometer is reported alongside accel/gyro in the same IMU group and exposes the same
    // ODR / full-scale-range preset lists, so build one profile per (range, rate) combination.
    bool magPresetsLoaded = false;
    if(propServer->isPropertySupported(OB_STRUCT_GET_MAG_PRESETS_ODR_LIST, PROP_OP_READ, PROP_ACCESS_INTERNAL)
       && propServer->isPropertySupported(OB_STRUCT_GET_MAG_PRESETS_FULL_SCALE_LIST, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
        // The properties are registered but the firmware may still not answer them (older firmware). Keep the
        // sensor creatable in that case instead of letting the exception break the whole device bring-up.
        try {
            auto magSampleRateList     = propServer->getStructureDataT<MagSampleRateList>(OB_STRUCT_GET_MAG_PRESETS_ODR_LIST);
            auto magFullScaleRangeList = propServer->getStructureDataT<MagFullScaleRangeList>(OB_STRUCT_GET_MAG_PRESETS_FULL_SCALE_LIST);

            // Raw preset lists as reported by firmware. These values are exchanged with the device directly, so
            // they are logged at debug level to make device bring-up and firmware integration easier.
            LOG_DEBUG("Mag presets from firmware: odrCount={}, fullScaleRangeCount={}", magSampleRateList.num, magFullScaleRangeList.num);
            for(uint32_t i = 0; i < magSampleRateList.num; i++) {
                LOG_DEBUG("  mag odr[{}] = {}", i, static_cast<int>(magSampleRateList.odr[i]));
            }
            for(uint32_t j = 0; j < magFullScaleRangeList.num; j++) {
                LOG_DEBUG("  mag fullScaleRange[{}] = {}", j, static_cast<int>(magFullScaleRangeList.fs[j]));
            }

            auto lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_MAG);
            for(uint32_t i = 0; i < magSampleRateList.num; i++) {
                for(uint32_t j = 0; j < magFullScaleRangeList.num; j++) {
                    auto mag_odr = magSampleRateList.odr[i];
                    auto mag_fs  = magFullScaleRangeList.fs[j];
                    if(mag_odr == 0 || mag_fs == 0) {
                        continue;
                    }

                    auto profile = StreamProfileFactory::createMagStreamProfile(lazySensor, mag_fs, mag_odr);
                    streamProfileList_.emplace_back(profile);
                }
            }

            if(!streamProfileList_.empty()) {
                LOG_DEBUG("Mag built {} stream profiles from the firmware presets", streamProfileList_.size());

                if(propServer->isPropertySupported(OB_PROP_MAG_ODR_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)
                   && propServer->isPropertySupported(OB_PROP_MAG_FULL_SCALE_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
                    auto currentSampleRate = propServer->getPropertyValueT<OBMagSampleRate>(OB_PROP_MAG_ODR_INT);
                    auto currentFullScale  = propServer->getPropertyValueT<OBMagFullScaleRange>(OB_PROP_MAG_FULL_SCALE_INT);
                    LOG_DEBUG("Mag current odr / fullScaleRange from firmware: odr={}, fullScaleRange={}", static_cast<int>(currentSampleRate), static_cast<int>(currentFullScale));
                    auto defaultProfile = StreamProfileFactory::createMagStreamProfile(lazySensor, currentFullScale, currentSampleRate);
                    updateDefaultStreamProfile(defaultProfile);
                }
                else {
                    LOG_WARN("Mag odr / full-scale-range properties are not readable, the default stream profile is not updated from firmware");
                }

                magPresetsLoaded = true;
            }
            else {
                LOG_WARN("No valid mag stream profile was built from the firmware preset lists, check the odr / full-scale-range values reported by the device");
            }
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to read mag presets from firmware: {}, falling back to a single stream profile", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to read mag presets from firmware, falling back to a single stream profile");
        }
    }

    if(!magPresetsLoaded) {
        // Devices without usable magnetometer presets: describe the sensor with a single stream profile whose
        // sample rate mirrors the accelerometer's when available, otherwise leave it unknown. The range
        // falls back to the smallest supported value instead of the "any/unknown" sentinel so that the
        // profile always carries a valid enum that can be written back to the device and matched by name.
        LOG_WARN("Mag preset lists are unavailable, falling back to a single stream profile");
        streamProfileList_.clear();
        auto            lazySensor = std::make_shared<LazySensor>(owner, OB_SENSOR_MAG);
        OBIMUSampleRate sampleRate = OB_SAMPLE_RATE_UNKNOWN;
        if(propServer->isPropertySupported(OB_PROP_ACCEL_ODR_INT, PROP_OP_READ, PROP_ACCESS_INTERNAL)) {
            sampleRate = propServer->getPropertyValueT<OBIMUSampleRate>(OB_PROP_ACCEL_ODR_INT);
        }
        LOG_DEBUG("Mag fallback profile: odr={} (mirrored from accel when available), fullScaleRange={}", static_cast<int>(sampleRate), static_cast<int>(OB_MAG_FS_4G));

        auto profile = StreamProfileFactory::createMagStreamProfile(lazySensor, OB_MAG_FS_4G, sampleRate);
        streamProfileList_.emplace_back(profile);
        updateDefaultStreamProfile(profile);
    }

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

    auto owner = getOwner();
    if(enableStreamStrategyValidation_) {
        auto strategy = owner->getComponentT<ISensorStreamStrategy>(OB_DEV_COMPONENT_SENSOR_STREAM_STRATEGY, false);
        if(strategy) {
            strategy->validateStream(sp);
        }
    }

    activatedStreamProfile_ = sp;
    frameCallback_          = callback;
    updateStreamState(STREAM_STATE_STARTING);

    auto propServer = owner->getPropertyServer();

    // Apply the ODR / full-scale range carried by the profile, then enable the magnetometer, mirroring
    // what the accelerometer and gyroscope sensors do. The magnetometer properties are optional: a device
    // (or older firmware) that does not expose them keeps the profile reported by the device, and a missing
    // property must never stop the stream from starting. A real write failure still aborts the start and
    // rolls the state back so the sensor never stays stuck in STARTING.
    BEGIN_TRY_EXECUTE({
        auto magSp = sp->as<MagStreamProfile>();
        if(propServer->isPropertySupported(OB_PROP_MAG_ODR_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_ODR_INT, static_cast<int>(magSp->getSampleRate()));
        }
        if(propServer->isPropertySupported(OB_PROP_MAG_FULL_SCALE_INT, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_FULL_SCALE_INT, static_cast<int>(magSp->getFullScaleRange()));
        }
        if(propServer->isPropertySupported(OB_PROP_MAG_SWITCH_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_SWITCH_BOOL, true);
        }
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        activatedStreamProfile_.reset();
        frameCallback_ = nullptr;
        updateStreamState(STREAM_STATE_START_FAILED);
        throw;
    })

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
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();
    // Best effort: only clear the magnetometer switch where the device exposes it, and never let a property
    // failure turn into a stop failure (accel/gyro always register their switch, the magnetometer may not).
    TRY_EXECUTE({
        if(propServer->isPropertySupported(OB_PROP_MAG_SWITCH_BOOL, PROP_OP_WRITE, PROP_ACCESS_INTERNAL)) {
            propServer->setPropertyValueT(OB_PROP_MAG_SWITCH_BOOL, false);
        }
    })
    updateStreamState(STREAM_STATE_STOPPED);
}

}  // namespace libobsensor

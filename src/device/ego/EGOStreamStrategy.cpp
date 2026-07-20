// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOStreamStrategy.hpp"

#include "component/property/InternalProperty.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "stream/StreamProfile.hpp"
#include "utils/PublicTypeHelper.hpp"
#include "utils/Utils.hpp"

#include <algorithm>
#include <cstring>

namespace libobsensor {

EGOStreamStrategy::EGOStreamStrategy(IDevice *owner) : DeviceComponentBase(owner) {}

void EGOStreamStrategy::validateStream(const std::shared_ptr<const StreamProfile> &profile) {
    validateStream(std::vector<std::shared_ptr<const StreamProfile>>{ profile });
}

void EGOStreamStrategy::validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) {
    bool recording = false;
    try {
        recording = isRecording();
    }
    catch(const std::exception &e) {
        // Firmware command ids are still being aligned. Keep current streaming behavior when the
        // recording-state query is not available yet.
        LOG_WARN("failed to query SD recording state before stream start, allow streaming. reason: {}", e.what());
        return;
    }

    if(!recording) {
        return;
    }

    std::vector<OBInternalStreamProfile> recordingProfiles;
    try {
        recordingProfiles = fetchRecordingProfiles();
    }
    catch(const std::exception &e) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION(utils::string::to_string()
                                              << "Device is recording to SD card, but failed to get the recording profile list. reason: " << e.what());
    }

    if(recordingProfiles.empty()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Device is recording to SD card, but the recording profile list is empty.");
    }

    for(const auto &profile: profiles) {
        validateRecordingProfile(profile, recordingProfiles);
    }
}

void EGOStreamStrategy::markStreamActivated(const std::shared_ptr<const StreamProfile> &profile) {
    (void)profile;
}

void EGOStreamStrategy::markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile) {
    (void)profile;
}

bool EGOStreamStrategy::isRecording() const {
    auto propServer = getOwner()->getPropertyServer();
    return propServer->getPropertyValueT<bool>(OB_PROP_FW_RECORDING_STATE_BOOL, PROP_ACCESS_INTERNAL);
}

std::vector<OBInternalStreamProfile> EGOStreamStrategy::fetchRecordingProfiles() const {
    auto propServer = getOwner()->getPropertyServer();

    std::vector<uint8_t> data;
    propServer->getRawData(
        OB_RAW_DATA_FW_RECORDING_PROFILE_LIST,
        [&](OBDataTranState state, OBDataChunk *dataChunk) {
            if(state == DATA_TRAN_STAT_TRANSFERRING) {
                data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
            }
        },
        PROP_ACCESS_INTERNAL);

    if(data.empty()) {
        return {};
    }

    if(data.size() % sizeof(OBInternalStreamProfile) != 0) {
        THROW_INVALID_DATA_EXCEPTION("Invalid firmware recording profile list size");
    }

    std::vector<OBInternalStreamProfile> profiles(data.size() / sizeof(OBInternalStreamProfile));
    std::memcpy(profiles.data(), data.data(), data.size());
    return profiles;
}

void EGOStreamStrategy::validateRecordingProfile(const std::shared_ptr<const StreamProfile> &profile,
                                                  const std::vector<OBInternalStreamProfile> &recordingProfiles) const {
    if(!profile) {
        return;
    }

    auto sensorType = utils::mapStreamTypeToSensorType(profile->getType());
    if(sensorType == OB_SENSOR_UNKNOWN) {
        return;
    }

    auto sameSensor = [sensorType](const OBInternalStreamProfile &item) { return item.sensorType == static_cast<uint16_t>(sensorType); };
    if(std::none_of(recordingProfiles.begin(), recordingProfiles.end(), sameSensor)) {
        // This sensor is not participating in the current SD recording and can be opened normally.
        return;
    }

    if(profile->is<VideoStreamProfile>()) {
        auto vsp          = profile->as<VideoStreamProfile>();
        auto width        = vsp->getWidth();
        auto height       = vsp->getHeight();
        auto fps          = vsp->getFps();
        auto formatFourcc = utils::obFormatToUvcFourcc(vsp->getFormat());

        auto matched = std::any_of(recordingProfiles.begin(), recordingProfiles.end(), [&](const OBInternalStreamProfile &item) {
            return sameSensor(item) && item.profile.video.width == width && item.profile.video.height == height && item.profile.video.fps == fps
                   && item.profile.video.formatFourcc == formatFourcc;
        });
        if(matched) {
            return;
        }

        THROW_UNSUPPORTED_OPERATION_EXCEPTION(
            utils::string::to_string() << "Device is recording to SD card. The video stream profile must match the SD recording profile. stream="
                                       << utils::obStreamToStr(profile->getType()) << ", requested=" << width << "x" << height << "@" << fps
                                       << "fps, format=" << utils::obFormatToStr(vsp->getFormat()));
    }

    if(profile->is<AccelStreamProfile>()) {
        auto accel          = profile->as<AccelStreamProfile>();
        auto fullScaleRange = static_cast<uint16_t>(accel->getFullScaleRange());
        auto sampleRate     = static_cast<uint16_t>(accel->getSampleRate());

        auto matched = std::any_of(recordingProfiles.begin(), recordingProfiles.end(), [&](const OBInternalStreamProfile &item) {
            return sameSensor(item) && item.profile.accel.fullScaleRange == fullScaleRange && item.profile.accel.sampleRate == sampleRate;
        });
        if(matched) {
            return;
        }

        THROW_UNSUPPORTED_OPERATION_EXCEPTION(
            utils::string::to_string() << "Device is recording to SD card. The IMU stream profile must match the SD recording profile. stream="
                                       << utils::obStreamToStr(profile->getType()) << ", sampleRate=" << utils::obImuRateToStr(accel->getSampleRate())
                                       << ", fullScaleRange=" << utils::AccelFullScaleRangeToStr(accel->getFullScaleRange()));
    }

    if(profile->is<GyroStreamProfile>()) {
        auto gyro           = profile->as<GyroStreamProfile>();
        auto fullScaleRange = static_cast<uint16_t>(gyro->getFullScaleRange());
        auto sampleRate     = static_cast<uint16_t>(gyro->getSampleRate());

        auto matched = std::any_of(recordingProfiles.begin(), recordingProfiles.end(), [&](const OBInternalStreamProfile &item) {
            return sameSensor(item) && item.profile.gyro.fullScaleRange == fullScaleRange && item.profile.gyro.sampleRate == sampleRate;
        });
        if(matched) {
            return;
        }

        THROW_UNSUPPORTED_OPERATION_EXCEPTION(
            utils::string::to_string() << "Device is recording to SD card. The IMU stream profile must match the SD recording profile. stream="
                                       << utils::obStreamToStr(profile->getType()) << ", sampleRate=" << utils::obImuRateToStr(gyro->getSampleRate())
                                       << ", fullScaleRange=" << utils::GyroFullScaleRangeToStr(gyro->getFullScaleRange()));
    }
}

}  // namespace libobsensor

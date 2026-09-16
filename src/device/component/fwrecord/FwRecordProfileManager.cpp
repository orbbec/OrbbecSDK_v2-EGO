// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FwRecordProfileManager.hpp"

#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "exception/ObException.hpp"
#include "property/InternalProperty.hpp"
#include "utils/PublicTypeHelper.hpp"

#include <cstring>
#include <vector>

namespace libobsensor {
namespace {

bool isFwRecordVideoSensor(uint16_t sensorType) {
    switch(static_cast<OBSensorType>(sensorType)) {
    case OB_SENSOR_IR:
    case OB_SENSOR_COLOR:
    case OB_SENSOR_DEPTH:
    case OB_SENSOR_IR_LEFT:
    case OB_SENSOR_IR_RIGHT:
    case OB_SENSOR_RAW_PHASE:
    case OB_SENSOR_CONFIDENCE:
    case OB_SENSOR_COLOR_LEFT:
    case OB_SENSOR_COLOR_RIGHT:
    case OB_SENSOR_COLOR_SIDE_LEFT:
    case OB_SENSOR_COLOR_SIDE_RIGHT:
    case OB_SENSOR_COLOR_TOP:
        return true;
    default:
        return false;
    }
}

OBInternalStreamProfile toInternalFwRecordProfile(const ob_fw_record_stream_profile &profile) {
    OBInternalStreamProfile internalProfile{};
    internalProfile.sensorType = profile.sensorType;

    if(isFwRecordVideoSensor(profile.sensorType)) {
        internalProfile.profile.video.width        = profile.profile.video.width;
        internalProfile.profile.video.height       = profile.profile.video.height;
        internalProfile.profile.video.fps          = profile.profile.video.fps;
        internalProfile.profile.video.formatFourcc = utils::obFormatToUvcFourcc(static_cast<OBFormat>(profile.profile.video.formatType));
    }
    else if(profile.sensorType == OB_SENSOR_ACCEL) {
        internalProfile.profile.accel.fullScaleRange = profile.profile.accel.fullScaleRange;
        internalProfile.profile.accel.sampleRate     = profile.profile.accel.sampleRate;
    }
    else if(profile.sensorType == OB_SENSOR_GYRO) {
        internalProfile.profile.gyro.fullScaleRange = profile.profile.gyro.fullScaleRange;
        internalProfile.profile.gyro.sampleRate     = profile.profile.gyro.sampleRate;
    }
    else {
        std::memcpy(&internalProfile.profile, &profile.profile, sizeof(internalProfile.profile));
    }

    return internalProfile;
}

ob_fw_record_stream_profile fromInternalFwRecordProfile(const OBInternalStreamProfile &internalProfile) {
    ob_fw_record_stream_profile profile{};
    profile.sensorType = internalProfile.sensorType;

    if(isFwRecordVideoSensor(internalProfile.sensorType)) {
        profile.profile.video.width  = internalProfile.profile.video.width;
        profile.profile.video.height = internalProfile.profile.video.height;
        profile.profile.video.fps    = internalProfile.profile.video.fps;
        auto format                  = utils::uvcFourccToOBFormat(internalProfile.profile.video.formatFourcc);
        if(format == OB_FORMAT_HEVC) {
            format = OB_FORMAT_H265;
        }
        profile.profile.video.formatType = format;
    }
    else if(internalProfile.sensorType == OB_SENSOR_ACCEL) {
        profile.profile.accel.fullScaleRange = internalProfile.profile.accel.fullScaleRange;
        profile.profile.accel.sampleRate     = internalProfile.profile.accel.sampleRate;
    }
    else if(internalProfile.sensorType == OB_SENSOR_GYRO) {
        profile.profile.gyro.fullScaleRange = internalProfile.profile.gyro.fullScaleRange;
        profile.profile.gyro.sampleRate     = internalProfile.profile.gyro.sampleRate;
    }
    else {
        std::memcpy(&profile.profile, &internalProfile.profile, sizeof(profile.profile));
    }

    return profile;
}

}  // namespace

FwRecordProfileManager::FwRecordProfileManager(IDevice *owner) : DeviceComponentBase(owner) {}

void FwRecordProfileManager::getProfileList(ob_fw_record_stream_profile *profiles, uint32_t *profileCount) {
    auto propServer = getOwner()->getPropertyServer();

    std::vector<uint8_t> firmwareData;
    propServer->getRawData(
        OB_RAW_DATA_FW_RECORDING_PROFILE_LIST,
        [&](OBDataTranState state, OBDataChunk *dataChunk) {
            if(state == DATA_TRAN_STAT_TRANSFERRING) {
                firmwareData.insert(firmwareData.end(), dataChunk->data, dataChunk->data + dataChunk->size);
            }
        },
        PROP_ACCESS_INTERNAL);

    if(firmwareData.size() % sizeof(OBInternalStreamProfile) != 0) {
        THROW_INVALID_DATA_EXCEPTION("Invalid firmware recording profile list size");
    }

    auto actualCount = static_cast<uint32_t>(firmwareData.size() / sizeof(OBInternalStreamProfile));
    if(profiles == nullptr) {
        *profileCount = actualCount;
        return;
    }

    if(*profileCount < actualCount) {
        *profileCount = actualCount;
        THROW_INVALID_PARAM_EXCEPTION("Insufficient profile buffer capacity");
    }

    auto internalProfiles = reinterpret_cast<const OBInternalStreamProfile *>(firmwareData.data());
    for(uint32_t i = 0; i < actualCount; ++i) {
        profiles[i] = fromInternalFwRecordProfile(internalProfiles[i]);
    }
    *profileCount = actualCount;
}

void FwRecordProfileManager::setProfileList(const ob_fw_record_stream_profile *profiles, uint32_t profileCount) {
    if(profileCount == 0) {
        THROW_INVALID_PARAM_EXCEPTION("Firmware recording profile count must be greater than zero");
    }

    std::vector<OBInternalStreamProfile> internalProfiles(profileCount);
    for(uint32_t i = 0; i < profileCount; ++i) {
        internalProfiles[i] = toInternalFwRecordProfile(profiles[i]);
    }

    auto                 dataSize = internalProfiles.size() * sizeof(OBInternalStreamProfile);
    std::vector<uint8_t> data(dataSize);
    std::memcpy(data.data(), internalProfiles.data(), dataSize);

    auto propServer = getOwner()->getPropertyServer();
    propServer->setStructureData(OB_RAW_DATA_FW_RECORDING_PROFILE_LIST, data, PROP_ACCESS_INTERNAL);
}

}  // namespace libobsensor

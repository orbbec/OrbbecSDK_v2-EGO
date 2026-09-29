// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "ImuPayloadDecoder.hpp"

#include <cstring>

namespace libobsensor {

DecodedImuSample LegacyImuCalculatorDecoder::decode(const uint8_t *group, uint8_t accelFsr, uint8_t gyroFsr) const {
    const OBImuOriginData *raw = reinterpret_cast<const OBImuOriginData *>(group);

    DecodedImuSample sample;
    sample.accel[0]     = calculator_->calculateAccelGravity(static_cast<int16_t>(raw->accelX), accelFsr);
    sample.accel[1]     = calculator_->calculateAccelGravity(static_cast<int16_t>(raw->accelY), accelFsr);
    sample.accel[2]     = calculator_->calculateAccelGravity(static_cast<int16_t>(raw->accelZ), accelFsr);
    sample.gyro[0]      = calculator_->calculateGyroDPS(static_cast<int16_t>(raw->gyroX), gyroFsr);
    sample.gyro[1]      = calculator_->calculateGyroDPS(static_cast<int16_t>(raw->gyroY), gyroFsr);
    sample.gyro[2]      = calculator_->calculateGyroDPS(static_cast<int16_t>(raw->gyroZ), gyroFsr);
    sample.temperature  = calculator_->calculateRegisterTemperature(raw->temperature);
    sample.timestamp[0] = raw->timestamp[0];
    sample.timestamp[1] = raw->timestamp[1];
    sample.hasMag       = false;
    return sample;
}

DecodedImuSample DMC640Decoder::decode(const uint8_t *group, uint8_t /*accelFsr*/, uint8_t /*gyroFsr*/) const {
    // Copy into a local to avoid unaligned float access on the packed wire buffer.
    OBDMC640ImuOriginData raw;
    std::memcpy(&raw, group, sizeof(raw));

    DecodedImuSample sample;
    // Firmware already outputs physical quantities; pass through directly.
    sample.accel[0]     = raw.accX;
    sample.accel[1]     = raw.accY;
    sample.accel[2]     = raw.accZ;
    sample.gyro[0]      = raw.gyroX;
    sample.gyro[1]      = raw.gyroY;
    sample.gyro[2]      = raw.gyroZ;
    sample.temperature  = raw.temperature;
    sample.timestamp[0] = raw.timestamp[0];
    sample.timestamp[1] = raw.timestamp[1];
    sample.hasMag       = true;
    sample.mag[0]       = raw.magX;
    sample.mag[1]       = raw.magY;
    sample.mag[2]       = raw.magZ;
    // TODO: raw.anglePitch / raw.angleRoll / raw.angleYaw are available but not exposed externally yet.
    return sample;
}

DecodedImuSample HI05R2Decoder::decode(const uint8_t *group, uint8_t /*accelFsr*/, uint8_t /*gyroFsr*/) const {
    // Copy into a local to avoid unaligned float access on the packed wire buffer.
    OBHI05R2ImuOriginData raw;
    std::memcpy(&raw, group, sizeof(raw));

    DecodedImuSample sample;
    // Firmware already outputs physical quantities; pass through directly.
    sample.accel[0]     = raw.accX;
    sample.accel[1]     = raw.accY;
    sample.accel[2]     = raw.accZ;
    sample.gyro[0]      = raw.gyroX;
    sample.gyro[1]      = raw.gyroY;
    sample.gyro[2]      = raw.gyroZ;
    sample.temperature  = raw.temperature;
    sample.timestamp[0] = raw.timestamp[0];
    sample.timestamp[1] = raw.timestamp[1];
    sample.hasMag       = false;
    // TODO: raw.anglePitch / raw.angleRoll / raw.angleYaw are available but not exposed externally yet.
    return sample;
}

}  // namespace libobsensor

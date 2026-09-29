// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ImuCalculator.hpp"

#include <cstdint>
#include <cstddef>
#include <memory>

namespace libobsensor {

// Original imu data, software packaging method, needs to be calculated on the sdk side
typedef struct {
    int16_t  groupId;  // The number of groups in a pack
    int16_t  accelX;
    int16_t  accelY;
    int16_t  accelZ;
    int16_t  gyroX;
    int16_t  gyroY;
    int16_t  gyroZ;
    int16_t  temperature;
    uint32_t timestamp[2];
} OBImuOriginData;

#pragma pack(push, 1)
// Raw magnetometer data group carried by a reportId=6 HID packet. magX/Y/Z are int16 register
// values; the SDK converts them to physical quantities (uT) using the active full-scale range.
// 24 bytes per group, matching OBImuOriginData / Mt6826sAngleReport.
typedef struct {
    int16_t  groupId;  // group index inside the pack
    int16_t  magX;
    int16_t  magY;
    int16_t  magZ;
    uint32_t timestamp[2];
    uint8_t  reserved[8];
} OBMagnetometerData;
#pragma pack(pop)

static_assert(sizeof(OBMagnetometerData) == 24, "OBMagnetometerData must be 24 bytes");

#pragma pack(push, 1)
// DMC640 imu data, firmware outputs physical quantities directly (float), plus magnetometer,
// attitude angles, temperature and timestamp. 64 bytes per group.
typedef struct {
    int16_t  groupId;
    int16_t  reserved;
    float    anglePitch;
    float    angleRoll;
    float    angleYaw;
    float    accX;
    float    accY;
    float    accZ;
    float    gyroX;
    float    gyroY;
    float    gyroZ;
    float    magX;
    float    magY;
    float    magZ;
    float    temperature;
    uint32_t timestamp[2];
} OBDMC640ImuOriginData;
#pragma pack(pop)

static_assert(sizeof(OBDMC640ImuOriginData) == 64, "OBDMC640ImuOriginData must be 64 bytes to match the DMC640 wire format");

#pragma pack(push, 1)
// HI05R2 imu data (EgoPro EC130), firmware outputs physical quantities directly (float), plus attitude
// angles, temperature and timestamp. No magnetometer. 52 bytes per group.
typedef struct {
    int16_t  groupId;
    int16_t  reserved;
    float    anglePitch;
    float    angleRoll;
    float    angleYaw;
    float    accX;
    float    accY;
    float    accZ;
    float    gyroX;
    float    gyroY;
    float    gyroZ;
    float    temperature;
    uint32_t timestamp[2];
} OBHI05R2ImuOriginData;
#pragma pack(pop)

static_assert(sizeof(OBHI05R2ImuOriginData) == 52, "OBHI05R2ImuOriginData must be 52 bytes to match the HI05R2 wire format");

// Decoded, unit-normalized IMU sample. Accel in g, gyro in dps, temperature in Celsius.
struct DecodedImuSample {
    float    accel[3]     = { 0, 0, 0 };
    float    gyro[3]      = { 0, 0, 0 };
    float    temperature  = 0;
    uint32_t timestamp[2] = { 0, 0 };
    bool     hasMag       = false;
    float    mag[3]       = { 0, 0, 0 };
};

// Abstraction that turns one raw IMU group (as laid out on the wire) into a DecodedImuSample.
// Injected into ImuStreamer per device class / PID.
class IImuPayloadDecoder {
public:
    virtual ~IImuPayloadDecoder() = default;

    // Size in bytes of one IMU group on the wire; used to stride the payload and validate header.groupLen.
    virtual size_t groupSize() const = 0;

    // Decode one group. accelFsr/gyroFsr are the full-scale-range codes required by register-raw decoders;
    // float-passthrough decoders ignore them.
    virtual DecodedImuSample decode(const uint8_t *group, uint8_t accelFsr, uint8_t gyroFsr) const = 0;
};

// Backward-compatible decoder for the legacy 24-byte int16 register format. Wraps an IImuCalculator so
// existing devices keep their exact behavior.
class LegacyImuCalculatorDecoder : public IImuPayloadDecoder {
public:
    explicit LegacyImuCalculatorDecoder(std::shared_ptr<IImuCalculator> calculator) : calculator_(std::move(calculator)) {}
    ~LegacyImuCalculatorDecoder() override = default;

    size_t groupSize() const override {
        return sizeof(OBImuOriginData);
    }

    DecodedImuSample decode(const uint8_t *group, uint8_t accelFsr, uint8_t gyroFsr) const override;

private:
    std::shared_ptr<IImuCalculator> calculator_;
};

// DMC640 decoder: firmware already outputs float physical quantities, so accel/gyro/temperature/timestamp
// are passed through directly and the magnetometer is populated. Attitude angles are read but not exposed.
class DMC640Decoder : public IImuPayloadDecoder {
public:
    DMC640Decoder()           = default;
    ~DMC640Decoder() override = default;

    size_t groupSize() const override {
        return sizeof(OBDMC640ImuOriginData);
    }

    DecodedImuSample decode(const uint8_t *group, uint8_t accelFsr, uint8_t gyroFsr) const override;
};

// HI05R2 decoder (EgoPro EC130): firmware already outputs float physical quantities, so accel/gyro/
// temperature/timestamp are passed through directly. Attitude angles are read but not exposed; no magnetometer.
class HI05R2Decoder : public IImuPayloadDecoder {
public:
    HI05R2Decoder()           = default;
    ~HI05R2Decoder() override = default;

    size_t groupSize() const override {
        return sizeof(OBHI05R2ImuOriginData);
    }

    DecodedImuSample decode(const uint8_t *group, uint8_t accelFsr, uint8_t gyroFsr) const override;
};

}  // namespace libobsensor

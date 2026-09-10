// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

// UMI Finger USB device: Ego-derived three-camera product (1 Color + 2 IR).
// Each camera occupies its own UVC streaming interface (1:1 endpoint -> stream, no demux).
// IMU (accel/gyro) and audio reuse the Ego HID/UAC links. Magnetometer is not yet implemented.
class UMIFingerDevice : public DeviceBase {
public:
    UMIFingerDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~UMIFingerDevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fetchDeviceInfo() override;

private:
    uint64_t deviceTimeFreq_ = 1000;
    uint64_t frameTimeFreq_  = 1000;
};

}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"
#include "stream/StreamProfile.hpp"

#include <map>
#include <memory>

namespace libobsensor {

// UMI Finger network device: Ego-derived three-camera product (1 Color + 2 IR) over RTSP
// (independent URL paths color/ir_left/ir_right), audio over RTSP, IMU over an independent
// private TCP data stream. Magnetometer is not yet implemented.
class UMIFingerNetDevice : public DeviceBase {
public:
    UMIFingerNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~UMIFingerNetDevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fetchAllVideoStreamProfileList();

    void fetchDeviceInfo() override;

private:
    uint64_t deviceTimeFreq_     = 1000;
    uint64_t colorFrameTimeFreq_ = 90000;

    StreamProfileList allVideoStreamProfileList_;  // fetched from device via vendor-specific protocol
    bool              inRecoveryMode_ = false;
};

}  // namespace libobsensor

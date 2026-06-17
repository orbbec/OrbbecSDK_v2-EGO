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

// PN2608_EGO WiFi device: dual independent RGB over RTSP (independent URL paths) +
// IMU over an independent private TCP data stream (modeled on FemtoMega net IMU).
// (Audio over RTSP is wired in a later phase; see TN2608 plan §5.3.)
class EGONetDevice : public DeviceBase {
public:
    EGONetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~EGONetDevice() noexcept override;

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
    uint64_t depthFrameTimeFreq_ = 1000000;  // [EGO-SIM] depth alias timebase for the Femto Mega net stand-in

    StreamProfileList allVideoStreamProfileList_;  // fetched from device via vendor-specific protocol
    bool              inRecoveryMode_ = false;
};

}  // namespace libobsensor

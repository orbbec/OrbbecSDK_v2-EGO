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

// EGODepth network device: evolves from EgoPro network device.
//   - Color streams (Left / Right / Top) + single depth-module Color, each over an independent RTSP path.
//   - Depth + dual IR (Left / Right), each over an independent RTSP path (output only).
//   - IMU over an independent private TCP data stream, Audio over RTSP.
// Unlike the USB variant there is no shared-endpoint multiplexing: every logical stream owns its own RTSP
// path, so plain VideoSensors are used (no demuxer).
class EGODepthNetDevice : public DeviceBase {
public:
    EGODepthNetDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~EGODepthNetDevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

    // See EGODepthDevice::postInitialize(): instantiating sensors from the constructor would make
    // FrameProcessorFactory's constructor throw std::bad_weak_ptr.
    void postInitialize() override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fixSensorList();                       // structural trimming only (deregister*), safe from init()
    void applyEffectiveStreamProfileFilters();  // instantiates sensors, postInitialize() only
    void fetchAllVideoStreamProfileList();

    void fetchDeviceInfo() override;

    // Register one plain RTSP video sensor identified by its stream type; no-op when the matching RTSP port
    // was not enumerated.
    void registerNetVideoSensor(OBStreamType streamType, OBSensorType sensorType, DeviceComponentId sensorComponent, DeviceComponentId frameProcessorComponent);

private:
    uint64_t deviceTimeFreq_     = 1000;
    uint64_t colorFrameTimeFreq_ = 90000;

    StreamProfileList allVideoStreamProfileList_;  // fetched from device via vendor-specific protocol
    bool              inRecoveryMode_ = false;
};

}  // namespace libobsensor

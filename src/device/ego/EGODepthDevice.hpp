// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "DeviceBase.hpp"
#include "IDeviceManager.hpp"
#include "frameprocessor/FrameProcessor.hpp"

#include <map>
#include <memory>

namespace libobsensor {

// EGODepth USB device: evolves from EgoPro.
//   - Color group : 3 Color cameras (Left / Right / Top) multiplexed on one shared UVC endpoint,
//                   demultiplexed by UVC SCR cam_id.
//   - IR group    : 2 IR cameras (Left / Right) multiplexed on a second shared UVC endpoint.
//   - Depth       : one exclusive UVC endpoint (1:1 passthrough, output only).
//   - Color       : one exclusive UVC endpoint (single Color of the depth module).
//   - IMU (HID) + Audio (UAC), same as EgoPro.
// A single vendor XU (on the Color-group interface) carries every command; the four Color streams are
// controlled together through it (firmware normalizes per-sensor).
class EGODepthDevice : public DeviceBase {
public:
    EGODepthDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    virtual ~EGODepthDevice() noexcept override;

    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType type) override;

private:
    void init() override;
    void initSensorList();
    void initProperties();
    void initSensorStreamProfile(std::shared_ptr<ISensor> sensor);
    void fixSensorList();
    void fetchDeviceInfo() override;

private:
    uint64_t deviceTimeFreq_ = 1000;
    uint64_t frameTimeFreq_  = 1000;
};

}  // namespace libobsensor

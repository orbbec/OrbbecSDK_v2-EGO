// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "DeviceBase.hpp"
#include "EgoProRkFirmwareUpdater.hpp"
#include <memory>
#include <vector>
namespace libobsensor {
// EgoPro device in RockUSB download (Loader) mode. Only firmware update is
// supported in this state; no streaming sensors are exposed.
class EgoProRkBootDevice : public DeviceBase {
public:
    explicit EgoProRkBootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    ~EgoProRkBootDevice() noexcept override;
    void                                  updateFirmware(const std::vector<uint8_t> &, DeviceFwUpdateCallback, bool) override;
    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType) override {
        return {};
    }

private:
    void                                     init() override;
    std::unique_ptr<EgoProRkFirmwareUpdater> updater_;
};
}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "DeviceBase.hpp"
#include "EGOBinFirmwareUpdater.hpp"
#include <memory>
#include <vector>
namespace libobsensor {
class EGOBootDevice : public DeviceBase {
public:
    explicit EGOBootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info);
    ~EGOBootDevice() noexcept override;
    void                                  updateFirmware(const std::vector<uint8_t> &, DeviceFwUpdateCallback, bool) override;
    std::vector<std::shared_ptr<IFilter>> createRecommendedPostProcessingFilters(OBSensorType) override {
        return {};
    }

private:
    void                                   init() override;
    std::unique_ptr<EGOBinFirmwareUpdater> updater_;
};
}  // namespace libobsensor

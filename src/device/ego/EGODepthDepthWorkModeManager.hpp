// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "InternalTypes.hpp"
#include "IDepthWorkModeManager.hpp"
#include "DeviceComponentBase.hpp"

namespace libobsensor {

constexpr const char *kEgoDepthDefaultMode      = "Default";
constexpr const char *kEgoDepthFactoryCalibMode = "Factory Calib";

class EGODepthDepthWorkModeManager : public IDepthWorkModeManager, public DeviceComponentBase {
public:
    EGODepthDepthWorkModeManager(IDevice *owner);
    virtual ~EGODepthDepthWorkModeManager() noexcept override = default;

    std::vector<OBDepthWorkMode_Internal> getDepthWorkModeList() const override;
    const OBDepthWorkMode_Internal       &getCurrentDepthWorkMode() const override;
    void                                  switchDepthWorkMode(const std::string &modeName) override;
    void                                  fetchDepthWorkModeList() override;

private:
    void switchDepthWorkMode(const OBDepthWorkMode_Internal &targetDepthMode);

private:
    std::vector<OBDepthWorkMode_Internal> depthWorkModeList_;
    OBDepthWorkMode_Internal              currentWorkMode_{};
};

}  // namespace libobsensor

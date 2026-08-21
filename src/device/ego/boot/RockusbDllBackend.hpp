// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "IRockusbUpgradeBackend.hpp"
#include <string>
namespace libobsensor {
// IRockusbUpgradeBackend implementation that calls the Rockchip upgrade-tool
// entry (egopro_rk_upgrade_file, see EgoProUpgradeApi.h) directly. The
// upgrade-tool sources are compiled into the SDK (static EgoProRkUpgrade
// target) and linked at build time, so there is no runtime library load.
class RockusbDllBackend : public IRockusbUpgradeBackend {
public:
    RockusbDllBackend();
    ~RockusbDllBackend() noexcept override;
    void upgrade(const std::string &imagePath, const ProgressCallback &progress, const std::atomic_bool &cancelled) override;
};
}  // namespace libobsensor

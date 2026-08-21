// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <string>
namespace libobsensor {
// Transport-agnostic seam for flashing an EgoPro device that is in RockUSB
// download (Loader) mode. Two implementations are planned:
//   1. RockusbDllBackend  - delegates to the Rockchip upgrade-tool source
//      (rockusb.sys on Windows, libusb elsewhere) compiled into the SDK.
//   2. an in-SDK libusb/WinUSB backend (self-developed) - future.
// The SDK only ever depends on this abstraction.
class IRockusbUpgradeBackend {
public:
    // percent in [0,100]; message is a short human-readable status.
    using ProgressCallback = std::function<void(int percent, const std::string &message)>;

    virtual ~IRockusbUpgradeBackend() = default;

    // Flash the update image at imagePath. Blocks until done or failure.
    // Throws on failure. Implementations must poll cancelled and abort with an
    // exception when it becomes true.
    virtual void upgrade(const std::string &imagePath, const ProgressCallback &progress, const std::atomic_bool &cancelled) = 0;
};

// Returns the backend used by default (phase 1: the RockusbDllBackend).
std::unique_ptr<IRockusbUpgradeBackend> createDefaultRockusbUpgradeBackend();
}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "IDevice.hpp"
#include "IRockusbUpgradeBackend.hpp"
#include "SourcePortInfo.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
namespace libobsensor {
// Firmware updater for an EgoPro device in RockUSB download (Loader) mode.
// The RockUSB transport and image parsing are handled by an
// IRockusbUpgradeBackend; this class only manages the worker thread, image
// staging and OBFwUpdateState reporting, mirroring EGOBinFirmwareUpdater.
class EgoProRkFirmwareUpdater {
public:
    EgoProRkFirmwareUpdater(std::shared_ptr<const USBSourcePortInfo> bootPortInfo, uint16_t pid);
    ~EgoProRkFirmwareUpdater() noexcept;
    void update(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback callback, bool async);
    void updateFromPath(const std::string &path, DeviceFwUpdateCallback callback, bool async);

private:
    void                                     run(std::vector<uint8_t> firmware, DeviceFwUpdateCallback callback);
    void                                     runFromPath(std::string path, DeviceFwUpdateCallback callback);
    std::shared_ptr<const USBSourcePortInfo> bootPortInfo_;
    uint16_t                                 initialPid_;
    std::unique_ptr<IRockusbUpgradeBackend>  backend_;
    std::thread                              worker_;
    std::atomic_bool                         cancelled_{ false };
};
}  // namespace libobsensor

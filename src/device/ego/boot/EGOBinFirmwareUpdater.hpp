// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "IDevice.hpp"
#include "SourcePortInfo.hpp"
#include "usb/scsi/ScsiDevicePort.hpp"
#include <atomic>
#include <thread>
#include <vector>
namespace libobsensor {
class EGOBinFirmwareUpdater {
public:
    EGOBinFirmwareUpdater(std::shared_ptr<const USBSourcePortInfo> bootPortInfo, uint16_t pid);
    ~EGOBinFirmwareUpdater() noexcept;
    void update(const std::vector<uint8_t> &firmware, DeviceFwUpdateCallback callback, bool async);

private:
    void                                     run(std::vector<uint8_t> firmware, DeviceFwUpdateCallback callback);
    std::shared_ptr<IScsiDevicePort>         openPort(std::shared_ptr<const USBSourcePortInfo> info);
    std::shared_ptr<const USBSourcePortInfo> waitForUbootPort();
    std::shared_ptr<const USBSourcePortInfo> bootPortInfo_;
    uint16_t                                 initialPid_;
    std::thread                              worker_;
    std::atomic_bool                         cancelled_{ false };
};
}  // namespace libobsensor

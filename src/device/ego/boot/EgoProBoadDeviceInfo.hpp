// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"
#include <memory>
#include <vector>
namespace libobsensor {
// EgoPro firmware-upgrade device in RockUSB download (Loader) mode.
// Exposed by the Rockchip "USB download gadget" (VID 0x2207 / PID 0x350E),
// a vendor-specific bulk interface. Only the firmware-update path is offered.
class EgoProBoadDeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<EgoProBoadDeviceInfo> {
public:
    explicit EgoProBoadDeviceInfo(SourcePortInfoList ports);
    std::shared_ptr<IDevice>                             createDevice(OBDeviceAccessMode accessMode = OB_DEVICE_DEFAULT_ACCESS) const override;
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList &ports);
};
}  // namespace libobsensor

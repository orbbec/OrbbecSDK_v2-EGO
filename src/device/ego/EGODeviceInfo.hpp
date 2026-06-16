// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <memory>

namespace libobsensor {

// Device enumeration info for the PN2608_EGO head-mounted device.
// Modeled on FemtoMegaDeviceInfo: a single createDevice() branches by connectionType
// to build either the USB (EGODevice) or WiFi (EGONetDevice) device class.
class EGODeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<EGODeviceInfo> {
public:
    EGODeviceInfo(const SourcePortInfoList groupedInfoList);
    ~EGODeviceInfo() noexcept override;

    std::shared_ptr<IDevice> createDevice(OBDeviceAccessMode accessMode) const override;
#if defined(BUILD_USB_PAL)
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList infoList);
#endif
#if defined(BUILD_NET_PAL)
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickNetDevices(const SourcePortInfoList infoList);
#endif
};

}  // namespace libobsensor

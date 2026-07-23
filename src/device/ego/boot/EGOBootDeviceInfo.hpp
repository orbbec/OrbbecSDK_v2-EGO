// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "devicemanager/DeviceEnumInfoBase.hpp"
#include <memory>
#include <vector>
namespace libobsensor {
class EGOBootDeviceInfo : public DeviceEnumInfoBase, public std::enable_shared_from_this<EGOBootDeviceInfo> {
public:
    explicit EGOBootDeviceInfo(SourcePortInfoList ports);
    std::shared_ptr<IDevice>                             createDevice(OBDeviceAccessMode accessMode = OB_DEVICE_DEFAULT_ACCESS) const override;
    static std::vector<std::shared_ptr<IDeviceEnumInfo>> pickDevices(const SourcePortInfoList &ports);
};
}  // namespace libobsensor

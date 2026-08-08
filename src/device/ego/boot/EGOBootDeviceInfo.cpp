// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOBootDeviceInfo.hpp"
#include "EGOBootDevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "utils/Utils.hpp"
#include <algorithm>
#include <iterator>
#include <utility>
namespace libobsensor {
EGOBootDeviceInfo::EGOBootDeviceInfo(SourcePortInfoList ports) {
    auto p              = std::dynamic_pointer_cast<const USBSourcePortInfo>(ports.front());
    pid_                = p->pid;
    vid_                = p->vid;
    uid_                = p->uid;
    deviceSn_           = p->serial;
    connectionType_     = "USB";
    sourcePortInfoList_ = std::move(ports);
    // Keep the two recovery stages visible to applications.  In particular,
    // a ROM device re-enumerates as U-Boot during an EGO firmware update.
    name_               = pid_ == EGO_BOOT_ROM_PID ? "Ego Boot ROM Device" : "Ego Boot U-Boot Device";
    fullName_           = name_;
}
std::shared_ptr<IDevice> EGOBootDeviceInfo::createDevice(OBDeviceAccessMode mode) const {
    (void)mode;
    return std::make_shared<EGOBootDevice>(shared_from_this());
}
std::vector<std::shared_ptr<IDeviceEnumInfo>> EGOBootDeviceInfo::pickDevices(const SourcePortInfoList &ports) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> out;
    auto                                          matched = FilterUSBPortInfoByVidPid(ports, EGO_BOOT_VID, EGOBootDevPids);
    SourcePortInfoList                            massStoragePorts;
    std::copy_if(matched.begin(), matched.end(), std::back_inserter(massStoragePorts),
                 [](const std::shared_ptr<const SourcePortInfo> &port) { return port->portType == SOURCE_PORT_USB_MASS_STORAGE; });
    auto groups = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(massStoragePorts, GroupUSBSourcePortByUrl);
    for(auto &g: groups) {
        if(!g.empty()) {
            out.push_back(std::make_shared<EGOBootDeviceInfo>(g));
        }
    }
    return out;
}
}  // namespace libobsensor

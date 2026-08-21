// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EgoProBoadDeviceInfo.hpp"
#include "EgoProRkBootDevice.hpp"
#include "DevicePids.hpp"
#include "usb/UsbPortGroup.hpp"
#include "utils/Utils.hpp"
#include <algorithm>
#include <iterator>
#include <utility>
namespace libobsensor {
EgoProBoadDeviceInfo::EgoProBoadDeviceInfo(SourcePortInfoList ports) {
    auto p              = std::dynamic_pointer_cast<const USBSourcePortInfo>(ports.front());
    pid_                = p->pid;
    vid_                = p->vid;
    uid_                = p->uid;
    deviceSn_           = p->serial;
    connectionType_     = "USB";
    sourcePortInfoList_ = std::move(ports);
    name_               = "EgoPro Boot Device";
    fullName_           = name_;
}
std::shared_ptr<IDevice> EgoProBoadDeviceInfo::createDevice(OBDeviceAccessMode mode) const {
    (void)mode;
    return std::make_shared<EgoProRkBootDevice>(shared_from_this());
}
std::vector<std::shared_ptr<IDeviceEnumInfo>> EgoProBoadDeviceInfo::pickDevices(const SourcePortInfoList &ports) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> out;
    const std::vector<uint16_t>                   pids    = { EGOPRO_BOADER_PID };
    auto                                          matched = FilterUSBPortInfoByVidPid(ports, EGOPRO_BOADER_VID, pids);
    // The RockUSB download gadget presents a single vendor-specific bulk
    // interface. On Windows it is bound to rockusb.sys, so libusb cannot open
    // it, but its cached descriptors still classify the port as VENDOR.
    SourcePortInfoList vendorPorts;
    std::copy_if(matched.begin(), matched.end(), std::back_inserter(vendorPorts),
                 [](const std::shared_ptr<const SourcePortInfo> &port) { return port->portType == SOURCE_PORT_USB_VENDOR; });
    auto groups = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(vendorPorts, GroupUSBSourcePortByUrl);
    for(auto &g: groups) {
        if(!g.empty()) {
            out.push_back(std::make_shared<EgoProBoadDeviceInfo>(g));
        }
    }
    return out;
}
}  // namespace libobsensor

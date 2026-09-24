// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "WristDeviceInfo.hpp"
#include "WristDevice.hpp"
#include "WristNetDevice.hpp"
#include "DevicePids.hpp"
#include "common/DeviceSeriesInfo.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <algorithm>
#include "SourcePortInfo.hpp"
#if defined(BUILD_USB_PAL)
#include "usb/UsbPortGroup.hpp"
#endif
#if defined(BUILD_NET_PAL)
#include "ethernet/NetPortGroup.hpp"
#include "ethernet/NetDataStreamPort.hpp"
#endif

namespace libobsensor {

static const uint16_t WRIST_COLOR_RTSP_PORT = 8888;
static const uint16_t WRIST_IMU_TCP_PORT    = 8900;  // private TCP IMU data stream

void updateWristDeviceName(uint32_t vid, uint32_t pid, std::string &name, std::string &fullName) {
    auto iter = std::find_if(WristDeviceInfoList.begin(), WristDeviceInfoList.end(),
                             [vid, pid](const DeviceInfoEntry &entry) { return entry.vid_ == vid && entry.pid_ == pid; });
    if(iter != WristDeviceInfoList.end()) {
        name     = iter->deviceName_;
        fullName = name;
    }
    else {
        name     = "Wrist series device";
        fullName = name;
    }
}

WristDeviceInfo::WristDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        updateWristDeviceName(portInfo->vid, portInfo->pid, name_, fullName_);
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        updateWristDeviceName(portInfo->vid, portInfo->pid, name_, fullName_);
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->mac;
        deviceSn_           = portInfo->serialNumber;
        connectionType_     = "Ethernet";
        sourcePortInfoList_ = groupedInfoList;
    }
    else {
        THROW_INVALID_PARAM_EXCEPTION("Invalid port type");
    }
}

WristDeviceInfo::~WristDeviceInfo() noexcept {}

std::shared_ptr<IDevice> WristDeviceInfo::createDevice(OBDeviceAccessMode accessMode) const {
    (void)accessMode;  // access control is unsupported
    std::shared_ptr<IDevice> device;
    if(connectionType_ == "Ethernet") {
        device = std::make_shared<WristNetDevice>(shared_from_this());
    }
    else {
        device = std::make_shared<WristDevice>(shared_from_this());
    }
    return device;
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> WristDeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> wristDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByVidPid(infoList, WristDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        auto uvcCount = static_cast<size_t>(std::count_if(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
            return p->portType == SOURCE_PORT_USB_UVC;
        }));
        bool hasHid = std::any_of(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
            return p->portType == SOURCE_PORT_USB_HID;
        });
        // Wrist USB layout: 1 UVC + 1 HID IMU. Do not create the device until both interfaces are present.
        if(uvcCount >= 1 && hasHid) {
            auto info = std::make_shared<WristDeviceInfo>(*iter);
            wristDeviceInfos.push_back(info);
        }
        iter++;
    }

    return wristDeviceInfos;
}
#endif

#if defined(BUILD_NET_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> WristDeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> wristDeviceInfos;
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, WristDevPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());

            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, WRIST_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR));

            // IMU over an independent private TCP data stream
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, WRIST_IMU_TCP_PORT, portInfo->port));

            auto deviceEnumInfo = std::make_shared<WristDeviceInfo>(*iter);
            wristDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return wristDeviceInfos;
}
#endif

}  // namespace libobsensor

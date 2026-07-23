// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "WristDeviceInfo.hpp"
#include "WristDevice.hpp"
#include "WristNetDevice.hpp"
#include "DevicePids.hpp"
#include "common/CommonFields.hpp"
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

const std::map<int, std::string> WristDeviceNameMap = {
    { OB_DEVICE_WRIST_PID, "Wrist" },
};

WristDeviceInfo::WristDeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        auto iter = WristDeviceNameMap.find(portInfo->pid);
        if(iter != WristDeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Wrist series device";
        }
        fullName_           = "Orbbec " + name_;
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        auto iter = WristDeviceNameMap.find(portInfo->pid);
        if(iter != WristDeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "Wrist series device";
        }
        fullName_           = "Orbbec " + name_;
        pid_                = portInfo->pid;
        vid_                = ORBBEC_DEVICE_VID;
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
    std::vector<uint16_t>                         wristPids = WristDevPids;
    auto                                          remainder = FilterUSBPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, wristPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        bool hasColor = std::any_of(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
            return p->portType == SOURCE_PORT_USB_UVC;
        });
        if(hasColor) {
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
    std::vector<uint16_t>                         wristPids = WristDevPids;
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, wristPids);
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

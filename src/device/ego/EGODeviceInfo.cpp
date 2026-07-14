// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODeviceInfo.hpp"
#include "EGODevice.hpp"
#include "EGONetDevice.hpp"
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

static const uint16_t EGO_RTSP_SERVICE_PORT = 8888;  // shared RTSP service port for all video/audio streams
static const uint16_t EGO_IMU_TCP_PORT      = 8900;  // private TCP IMU data stream (parallel to RTSP)

const std::map<int, std::string> EGODeviceNameMap = {
    { OB_DEVICE_EGO_PID, "Ego" },
};

EGODeviceInfo::EGODeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        auto iter = EGODeviceNameMap.find(portInfo->pid);
        if(iter != EGODeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "EGO series device";
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

        auto iter = EGODeviceNameMap.find(portInfo->pid);
        if(iter != EGODeviceNameMap.end()) {
            name_ = iter->second;
        }
        else {
            name_ = "EGO series device";
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

EGODeviceInfo::~EGODeviceInfo() noexcept {}

std::shared_ptr<IDevice> EGODeviceInfo::createDevice(OBDeviceAccessMode accessMode) const {
    (void)accessMode;  // access control is unsupported
    std::shared_ptr<IDevice> device;
    if(connectionType_ == "Ethernet") {
        device = std::make_shared<EGONetDevice>(shared_from_this());
    }
    else {
        device = std::make_shared<EGODevice>(shared_from_this());
    }
    return device;
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> EGODeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> egoDeviceInfos;
    std::vector<uint16_t>                         egoPids   = EGODevPids;
    auto                                          remainder = FilterUSBPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, egoPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        // EGO USB exposes 2 UVC color interfaces + 1 HID IMU interface (+ UAC audio).
        if(iter->size() >= 2) {
#if defined(_WIN32)
            // On Windows the UAC microphone is not surfaced by USB enumeration (usbaudio.sys owns it); inject a
            // synthetic UAC port so the WASAPI backend can pair the capture endpoint by USB VID/PID.
            auto firstUsb = std::dynamic_pointer_cast<const USBSourcePortInfo>(iter->front());
            if(firstUsb) {
                bool hasUac =
                    std::any_of(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) { return p->portType == SOURCE_PORT_USB_UAC; });
                if(!hasUac) {
                    auto uacPort      = std::make_shared<USBSourcePortInfo>(SOURCE_PORT_USB_UAC);
                    uacPort->url      = firstUsb->url;
                    uacPort->uid      = firstUsb->uid;
                    uacPort->vid      = firstUsb->vid;
                    uacPort->pid      = firstUsb->pid;
                    uacPort->serial   = firstUsb->serial;
                    uacPort->connSpec = firstUsb->connSpec;
                    iter->emplace_back(uacPort);
                }
            }
#endif
            auto info = std::make_shared<EGODeviceInfo>(*iter);
            egoDeviceInfos.push_back(info);
        }
        iter++;
    }

    return egoDeviceInfos;
}
#endif

#if defined(BUILD_NET_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> EGODeviceInfo::pickNetDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> egoDeviceInfos;
    std::vector<uint16_t>                         egoPids   = EGODevPids;
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, egoPids);
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());

            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_COLOR_LEFT));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_COLOR_RIGHT));

            // Audio is an independent RTSP stream
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_AUDIO));

            // IMU over an independent private TCP data stream
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, EGO_IMU_TCP_PORT, portInfo->port));

            auto deviceEnumInfo = std::make_shared<EGODeviceInfo>(*iter);
            egoDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return egoDeviceInfos;
}
#endif

}  // namespace libobsensor

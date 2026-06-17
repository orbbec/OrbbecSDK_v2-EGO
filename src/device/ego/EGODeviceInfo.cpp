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

// PN2608_EGO WiFi port plan (see TN2608 plan §4.2 / §8, scheme A):
// - color_left / color_right / audio share the same RTSP service port, distinguished by URL path.
// - IMU runs on an independent private TCP data-stream port (parallel to the RTSP service port).
// These values are placeholders pending firmware confirmation.
static const uint16_t EGO_RTSP_SERVICE_PORT = 8554;  // shared RTSP service port for all video/audio streams
static const uint16_t EGO_IMU_TCP_PORT      = 8900;  // private TCP IMU data stream (parallel to RTSP)

const std::map<int, std::string> EGODeviceNameMap = {
    { OB_DEVICE_EGO_PID, "PN2608 EGO" },
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
    std::vector<uint16_t>                         egoPids = EGODevPids;
#ifdef EGO_SIMULATE_WITH_FEMTO_MEGA  // [EGO-SIM] delete block + revert egoPids to EGODevPids directly
    // Also claim the USB Femto Mega stand-in so it is created as an EGODevice (see DevicePids.hpp).
    egoPids.push_back(EGO_SIM_FEMTO_MEGA_PID);
#endif
    auto remainder = FilterUSBPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, egoPids);
    auto groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto iter      = groups.begin();
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
                    auto uacPort = std::make_shared<USBSourcePortInfo>(SOURCE_PORT_USB_UAC);
                    uacPort->url = firstUsb->url;
                    uacPort->uid = firstUsb->uid;
#ifdef EGO_SIMULATE_WITH_FEMTO_MEGA  // [EGO-SIM] keep only the #else branch when removing simulation
                    // The Femto Mega stand-in has no microphone; pair an external USB microphone instead. WASAPI
                    // matches the capture endpoint by VID/PID, falling back to the endpoint friendly-name hint
                    // carried in infName (Windows audio endpoints often hide the USB VID/PID). See DevicePids.hpp.
                    uacPort->vid     = EGO_SIM_MIC_VID;
                    uacPort->pid     = EGO_SIM_MIC_PID;
                    uacPort->infName = EGO_SIM_MIC_NAME;
#else
                    uacPort->vid = firstUsb->vid;
                    uacPort->pid = firstUsb->pid;
#endif
                    uacPort->serial   = firstUsb->serial;
                    uacPort->connSpec = firstUsb->connSpec;
                    iter->emplace_back(uacPort);
                }
            }
#elif defined(EGO_SIMULATE_WITH_FEMTO_MEGA)  // [EGO-SIM] delete block when removing simulation
            // On libusb platforms (Linux / Android) the external USB microphone standing in for the EGO audio is
            // enumerated as its own USB device and was filtered out by VID/PID above. Locate its real UAC port
            // in the full infoList and pair it onto the simulated EGO device, so ObLibUacDevicePort can open the
            // real interface directly (it detaches the kernel snd-usb-audio driver, see ObLibUacDevicePort).
            // Android: match any UAC port (OTG mic may differ in VID/PID from the XWF-1080P stand-in).
            // Linux/Mac: match by specific VID/PID of the XWF-1080P stand-in.
            bool hasUac =
                std::any_of(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) { return p->portType == SOURCE_PORT_USB_UAC; });
            if(!hasUac) {
#if defined(__ANDROID__)
                auto micIter = std::find_if(infoList.begin(), infoList.end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
                    auto usb = std::dynamic_pointer_cast<const USBSourcePortInfo>(p);
                    return usb && usb->portType == SOURCE_PORT_USB_UAC;
                });
#else
                auto micIter = std::find_if(infoList.begin(), infoList.end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
                    auto usb = std::dynamic_pointer_cast<const USBSourcePortInfo>(p);
                    return usb && usb->portType == SOURCE_PORT_USB_UAC && usb->vid == EGO_SIM_MIC_VID && usb->pid == EGO_SIM_MIC_PID;
                });
#endif
                if(micIter != infoList.end()) {
                    iter->emplace_back(*micIter);
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
    std::vector<uint16_t>                         egoPids = EGODevPids;
#ifdef EGO_SIMULATE_WITH_FEMTO_MEGA  // [EGO-SIM] delete block + revert egoPids to EGODevPids directly
    // Also claim the network Femto Mega stand-in so it is created as an EGONetDevice (see DevicePids.hpp).
    egoPids.push_back(EGO_SIM_FEMTO_MEGA_PID);
#endif
    auto remainder = FilterNetPortInfoByVidPid(infoList, ORBBEC_DEVICE_VID, egoPids);
    auto groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());

#ifdef EGO_SIMULATE_WITH_FEMTO_MEGA  // [EGO-SIM] keep only the #else branch when removing simulation
            // The Femto Mega WiFi stand-in serves the real Femto Mega RTSP layout, not the (placeholder) EGO one,
            // so route its actual streams through the EGO net path: RTSP color on 8888 + depth on 8554 + private
            // TCP IMU on 8900 (matches FemtoMegaDeviceInfo::pickNetDevices). EGONetDevice aliases these to
            // OB_SENSOR_COLOR / OB_SENSOR_DEPTH so generic depth+color samples (ego_sample) stream unchanged. IR is
            // omitted (ego_sample does not use it); audio is dropped because Femto Mega serves no microphone stream.
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, static_cast<uint16_t>(8888), portInfo->port, OB_STREAM_COLOR));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, static_cast<uint16_t>(8554), portInfo->port, OB_STREAM_DEPTH));
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, EGO_IMU_TCP_PORT, portInfo->port));
#else
            // Two independent color streams: each gets its own RTSPStreamPortInfo / URL path / ObRTSPClient,
            // but shares the same RTSP service port (distinguished by URL path, see plan §4.2 scheme A).
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_COLOR_LEFT));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_COLOR_RIGHT));

            // Audio is an independent RTSP stream (independent URL path, own ObRTSPClient), sharing the
            // same RTSP service port as color (distinguished by URL path, see plan §5.3 scheme A).
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_SERVICE_PORT, portInfo->port, OB_STREAM_AUDIO));

            // IMU over an independent private TCP data stream (not RTSP), modeled on FemtoMega.
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, EGO_IMU_TCP_PORT, portInfo->port));
#endif

            auto deviceEnumInfo = std::make_shared<EGODeviceInfo>(*iter);
            egoDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return egoDeviceInfos;
}
#endif

}  // namespace libobsensor

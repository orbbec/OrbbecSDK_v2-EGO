// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODeviceInfo.hpp"
#include "EGODevice.hpp"
#include "EGONetDevice.hpp"
#include "EgoProDevice.hpp"
#include "EgoProNetDevice.hpp"
#include "EgoProEC130Device.hpp"
#include "EgoProEC130NetDevice.hpp"
#include "UMIFingerDevice.hpp"
#include "UMIFingerNetDevice.hpp"
#include "EGODepthDevice.hpp"
#include "EGODepthNetDevice.hpp"
#include "DevicePids.hpp"
#include "common/DeviceSeriesInfo.hpp"
#include "utils/Utils.hpp"
#include "exception/ObException.hpp"

#include <algorithm>
#include <map>
#include "SourcePortInfo.hpp"
#if defined(BUILD_USB_PAL)
#include "usb/UsbPortGroup.hpp"
#endif
#if defined(BUILD_NET_PAL)
#include "ethernet/NetPortGroup.hpp"
#include "ethernet/NetDataStreamPort.hpp"
#endif

namespace libobsensor {

static const uint16_t EGO_LEFT_COLOR_RTSP_PORT  = 8888;
static const uint16_t EGO_RIGHT_COLOR_RTSP_PORT = 8889;
static const uint16_t EGO_RTSP_AUDIO_PORT       = 8890;
static const uint16_t EGO_IMU_TCP_PORT          = 8900;  // private TCP IMU data stream (parallel to RTSP)
// EGODepth's DMC640 IMU packs up to 10 groups (64B each) plus the 8B HID header per TCP packet.
static const uint32_t EGODEPTH_IMU_TCP_PACKET_SIZE      = 328;
// EgoPro EC130's HI05R2 IMU packs 5 groups (52B each) plus the 8B HID header per TCP packet.
static const uint32_t EGOPRO_EC130_IMU_TCP_PACKET_SIZE = 268;
static const uint16_t EGOPRO_SIDE_LEFT_COLOR_RTSP_PORT  = 8891;
static const uint16_t EGOPRO_SIDE_RIGHT_COLOR_RTSP_PORT = 8892;
// EGODepth-only RTSP streams. Color group Left/Right reuse 8888/8889; Top color reuses 8891 and the single
// depth-module Color reuses 8892 (the EGOPro side ports are absent on EGODepth). Depth and dual IR each get
// their own port.
static const uint16_t EGODEPTH_TOP_COLOR_RTSP_PORT    = 8891;
static const uint16_t EGODEPTH_SINGLE_COLOR_RTSP_PORT = 8892;
static const uint16_t EGODEPTH_DEPTH_RTSP_PORT        = 8554;
static const uint16_t EGODEPTH_IR_LEFT_RTSP_PORT      = 8555;
static const uint16_t EGODEPTH_IR_RIGHT_RTSP_PORT     = 8556;

// UMI Finger three-camera network ports (Color / IR-Left / Audio / IR-Right / IMU); see design doc section 5.1.
static const uint16_t UMI_COLOR_RTSP_PORT    = 8888;
static const uint16_t UMI_IR_LEFT_RTSP_PORT  = 8889;
static const uint16_t UMI_RTSP_AUDIO_PORT    = 8890;
static const uint16_t UMI_IR_RIGHT_RTSP_PORT = 8891;
static const uint16_t UMI_IMU_TCP_PORT       = 8900;  // private TCP IMU data stream (accel/gyro/magnetometer)
static const uint32_t UMI_IMU_TCP_PACKET_SIZE =
    248;  // fixed framing: 8-byte HID header + up to ten 24-byte groups; firmware pads every reportId packet to this size

enum class EGOUsbDeviceSeries {
    EGO,
    EGOPro,
    EGODepth,
    UMIFinger,
};

static const std::map<EGOUsbDeviceSeries, size_t> kRequiredUvcCountBySeries = {
    { EGOUsbDeviceSeries::EGO, 2 },
    { EGOUsbDeviceSeries::EGOPro, 1 },
    { EGOUsbDeviceSeries::EGODepth, 4 },
    { EGOUsbDeviceSeries::UMIFinger, 3 },
};

// EGO, EGOPro and EGODepth are split into separate series lists (EGODevPids / EGOProDevPids / EGODepthDevPids)
// so that device-class selection is driven by list membership instead of a hardcoded PID. Enumeration still
// filters by the union of all lists.
static std::vector<DeviceIdentifier> getEgoSeriesPids() {
    std::vector<DeviceIdentifier> pids = EGODevPids;
    pids.insert(pids.end(), EGOProDevPids.begin(), EGOProDevPids.end());
    pids.insert(pids.end(), EGODepthDevPids.begin(), EGODepthDevPids.end());
    return pids;
}

void updateEgoDeviceName(uint32_t vid, uint32_t pid, std::string &name, std::string &fullName) {
    auto iter = std::find_if(EGODeviceInfoList.begin(), EGODeviceInfoList.end(),
                             [vid, pid](const DeviceInfoEntry &entry) { return entry.vid_ == vid && entry.pid_ == pid; });
    if(iter != EGODeviceInfoList.end()) {
        name     = iter->deviceName_;
        fullName = name;
    }
    else {
        name     = "Ego series device";
        fullName = name;
    }
}

EGODeviceInfo::EGODeviceInfo(const SourcePortInfoList groupedInfoList) {
    auto firstPortInfo = groupedInfoList.front();
    if(IS_USB_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(groupedInfoList.front());

        updateEgoDeviceName(portInfo->vid, portInfo->pid, name_, fullName_);
        pid_                = portInfo->pid;
        vid_                = portInfo->vid;
        uid_                = portInfo->uid;
        deviceSn_           = portInfo->serial;
        connectionType_     = portInfo->connSpec;
        sourcePortInfoList_ = groupedInfoList;
    }
    else if(IS_NET_PORT(firstPortInfo->portType)) {
        auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(groupedInfoList.front());

        updateEgoDeviceName(portInfo->vid, portInfo->pid, name_, fullName_);
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

EGODeviceInfo::~EGODeviceInfo() noexcept {}

std::shared_ptr<IDevice> EGODeviceInfo::createDevice(OBDeviceAccessMode accessMode) const {
    (void)accessMode;  // access control is unsupported
    std::shared_ptr<IDevice> device;
    if(connectionType_ == "Ethernet") {
        if(isEgoDepthDevice(vid_, pid_)) {
            device = std::make_shared<EGODepthNetDevice>(shared_from_this());
        }else if(pid_ == EGO_PRO_CUSTOM_PID) {
            device = std::make_shared<EgoProEC130NetDevice>(shared_from_this());
        }
        else if(isEgoProDevice(vid_, pid_)) {
            device = std::make_shared<EgoProNetDevice>(shared_from_this());
        }
        else if(pid_ == OB_DEVICE_UMI_PID) {
            device = std::make_shared<UMIFingerNetDevice>(shared_from_this());
        }
        else {
            device = std::make_shared<EGONetDevice>(shared_from_this());
        }
    }
    else {
        if(isEgoDepthDevice(vid_, pid_)) {
            device = std::make_shared<EGODepthDevice>(shared_from_this());
        }else if(pid_ == EGO_PRO_CUSTOM_PID) {
            device = std::make_shared<EgoProEC130Device>(shared_from_this());
        }
        else if(isEgoProDevice(vid_, pid_)) {
            device = std::make_shared<EgoProDevice>(shared_from_this());
        }
        else if(pid_ == OB_DEVICE_UMI_PID) {
            device = std::make_shared<UMIFingerDevice>(shared_from_this());
        }
        else {
            device = std::make_shared<EGODevice>(shared_from_this());
        }
    }
    return device;
}

#if defined(BUILD_USB_PAL)
std::vector<std::shared_ptr<IDeviceEnumInfo>> EGODeviceInfo::pickDevices(const SourcePortInfoList infoList) {
    std::vector<std::shared_ptr<IDeviceEnumInfo>> egoDeviceInfos;
    auto                                          remainder = FilterUSBPortInfoByVidPid(infoList, getEgoSeriesPids());
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupUSBSourcePortByUrl);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        // Require the video interfaces and HID IMU interface before creating the device. UAC is
        // intentionally excluded from this readiness check because Windows may add it later.
        auto firstUsb   = std::dynamic_pointer_cast<const USBSourcePortInfo>(iter->front());
        bool isEgoPro   = firstUsb && isEgoProDevice(firstUsb->vid, firstUsb->pid);
        bool isEgoDepth = firstUsb && isEgoDepthDevice(firstUsb->vid, firstUsb->pid);
        auto uvcCount = static_cast<size_t>(std::count_if(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
            return p->portType == SOURCE_PORT_USB_UVC;
        }));
        bool hasHid = std::any_of(iter->begin(), iter->end(), [](const std::shared_ptr<const SourcePortInfo> &p) {
            return p->portType == SOURCE_PORT_USB_HID;
        });

        EGOUsbDeviceSeries series = EGOUsbDeviceSeries::EGO;
        if(isEgoDepth) {
            series = EGOUsbDeviceSeries::EGODepth;
        }
        else if(isEgoPro) {
            series = EGOUsbDeviceSeries::EGOPro;
        }
        else if(firstUsb && firstUsb->pid == OB_DEVICE_UMI_PID) {
            series = EGOUsbDeviceSeries::UMIFinger;
        }

        // Expected USB layout is the configured UVC count plus one HID interface. UAC is not part
        // of this readiness check because Windows may add it later.
        const size_t requiredUvcCount = kRequiredUvcCountBySeries.at(series);
        bool           enoughPorts      = hasHid && uvcCount >= requiredUvcCount;
        if(enoughPorts) {
#if defined(_WIN32)
            // On Windows the UAC microphone is not surfaced by USB enumeration (usbaudio.sys owns it); inject a
            // synthetic UAC port so the WASAPI backend can pair the capture endpoint by USB VID/PID.
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
    auto                                          remainder = FilterNetPortInfoByVidPid(infoList, getEgoSeriesPids());
    auto                                          groups    = utils::groupVector<std::shared_ptr<const SourcePortInfo>>(remainder, GroupNetSourcePortByMac);
    auto                                          iter      = groups.begin();
    while(iter != groups.end()) {
        if(iter->size() >= 1) {
            auto portInfo = std::dynamic_pointer_cast<const NetSourcePortInfo>(iter->front());

            if(portInfo->pid == OB_DEVICE_UMI_PID) {
                // UMI Finger: 1 Color + 2 IR, each on its own RTSP port (1:1 stream -> port).
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, UMI_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, UMI_IR_LEFT_RTSP_PORT, portInfo->port, OB_STREAM_IR_LEFT));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, UMI_IR_RIGHT_RTSP_PORT, portInfo->port, OB_STREAM_IR_RIGHT));

                // Audio is an independent RTSP stream
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, UMI_RTSP_AUDIO_PORT, portInfo->port, OB_STREAM_AUDIO));

                // IMU (accel/gyro/magnetometer) over an independent private TCP data stream.
                iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, UMI_IMU_TCP_PORT, portInfo->port, UMI_IMU_TCP_PACKET_SIZE));

                auto deviceEnumInfo = std::make_shared<EGODeviceInfo>(*iter);
                egoDeviceInfos.push_back(deviceEnumInfo);
                iter++;
                continue;
            }

            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_LEFT_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR_LEFT));
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RIGHT_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR_RIGHT));
            if(isEgoDepthDevice(portInfo->vid, portInfo->pid)) {
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGODEPTH_TOP_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR_TOP));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGODEPTH_SINGLE_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGODEPTH_DEPTH_RTSP_PORT, portInfo->port, OB_STREAM_DEPTH));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGODEPTH_IR_LEFT_RTSP_PORT, portInfo->port, OB_STREAM_IR_LEFT));
                iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGODEPTH_IR_RIGHT_RTSP_PORT, portInfo->port, OB_STREAM_IR_RIGHT));
            }
            else if(isEgoProDevice(portInfo->vid, portInfo->pid)) {
                iter->emplace_back(
                    std::make_shared<RTSPStreamPortInfo>(*portInfo, EGOPRO_SIDE_LEFT_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR_SIDE_LEFT));
                iter->emplace_back(
                    std::make_shared<RTSPStreamPortInfo>(*portInfo, EGOPRO_SIDE_RIGHT_COLOR_RTSP_PORT, portInfo->port, OB_STREAM_COLOR_SIDE_RIGHT));
            }

            // Audio is an independent RTSP stream
            iter->emplace_back(std::make_shared<RTSPStreamPortInfo>(*portInfo, EGO_RTSP_AUDIO_PORT, portInfo->port, OB_STREAM_AUDIO));

            // IMU over an independent private TCP data stream
            uint32_t imuPacketSize = isEgoDepthDevice(portInfo->vid, portInfo->pid)
                                         ? EGODEPTH_IMU_TCP_PACKET_SIZE
                                         : portInfo->pid == EGO_PRO_CUSTOM_PID ? EGOPRO_EC130_IMU_TCP_PACKET_SIZE : OB_NET_DATA_STREAM_DEFAULT_PACKET_SIZE;
            iter->emplace_back(std::make_shared<NetDataStreamPortInfo>(*portInfo, EGO_IMU_TCP_PORT, portInfo->port, imuPacketSize));

            auto deviceEnumInfo = std::make_shared<EGODeviceInfo>(*iter);
            egoDeviceInfos.push_back(deviceEnumInfo);
        }
        iter++;
    }

    return egoDeviceInfos;
}
#endif

}  // namespace libobsensor

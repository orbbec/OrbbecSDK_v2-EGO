// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "WinScsiDevicePort.hpp"
#include "exception/ObException.hpp"
#include <ntddscsi.h>
#include <ntddstor.h>
#include <SetupAPI.h>
#include <cfgmgr32.h>
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cctype>
#pragma comment(lib, "SetupAPI.lib")
#pragma comment(lib, "Cfgmgr32.lib")
namespace libobsensor {
namespace {
struct ScsiPacket {
    SCSI_PASS_THROUGH spt;
    ULONG             filler;
    UCHAR             sense[32];
    UCHAR             data[1];
};
std::string normalizedInstanceUid(std::string id) {
    std::transform(id.begin(), id.end(), id.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto pos = id.find_last_of("\\");
    if(pos != std::string::npos) {
        id = id.substr(pos + 1);
    }
    id.erase(std::remove(id.begin(), id.end(), '&'), id.end());
    return id;
}
bool findUsbParent(DEVINST node, std::string &usbId) {
    for(int depth = 0; depth < 8; ++depth) {
        char id[MAX_DEVICE_ID_LEN]{};
        if(CM_Get_Device_IDA(node, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) {
            return false;
        }
        std::string current(id);
        std::string lower = current;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if(lower.find("usb\\vid_1b20&pid_0300") == 0 || lower.find("usb\\vid_1b20&pid_0301") == 0) {
            usbId = current;
            return true;
        }
        DEVINST parent = 0;
        if(CM_Get_Parent(&parent, node, 0) != CR_SUCCESS) {
            return false;
        }
        node = parent;
    }
    return false;
}
}  // namespace
WinScsiDevicePort::WinScsiDevicePort(std::shared_ptr<const USBSourcePortInfo> info) : portInfo_(std::move(info)) {
    if(!openEgoVolume()) {
        THROW_DEVICE_UNAVAILABLE_EXCEPTION("Unable to open EGO recovery mass-storage volume");
    }
}
WinScsiDevicePort::~WinScsiDevicePort() noexcept {
    if(handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
    }
}
bool WinScsiDevicePort::inquiryMatches(HANDLE candidate) {
    auto saved = handle_;
    handle_    = candidate;
    std::array<uint8_t, 36> data{};
    const uint8_t           cdb[6] = { 0x12, 0, 0, 0, 36, 0 };
    try {
        execute(cdb, 6, ScsiDataDirection::In, data.data(), 36, 2000);
    }
    catch(...) {
        handle_ = saved;
        return false;
    }
    handle_ = saved;
    return data[8] == 'G' && data[9] == 'C';
}
bool WinScsiDevicePort::openEgoVolume() {
    HDEVINFO devices = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(devices != INVALID_HANDLE_VALUE) {
        for(DWORD index = 0;; ++index) {
            SP_DEVICE_INTERFACE_DATA interfaceData{};
            interfaceData.cbSize = sizeof(interfaceData);
            if(!SetupDiEnumDeviceInterfaces(devices, nullptr, &GUID_DEVINTERFACE_DISK, index, &interfaceData)) {
                break;
            }
            DWORD           required = 0;
            SP_DEVINFO_DATA deviceData{};
            deviceData.cbSize = sizeof(deviceData);
            SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, nullptr, 0, &required, &deviceData);
            if(!required) {
                continue;
            }
            std::vector<uint8_t> detailBytes(required);
            auto                 detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailBytes.data());
            detail->cbSize              = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if(!SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, detail, required, nullptr, &deviceData)) {
                continue;
            }
            std::string usbParent;
            if(!findUsbParent(deviceData.DevInst, usbParent)) {
                continue;
            }
            if(!portInfo_->uid.empty() && normalizedInstanceUid(usbParent) != normalizedInstanceUid(portInfo_->uid)) {
                continue;
            }
            auto candidate =
                CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if(candidate != INVALID_HANDLE_VALUE && inquiryMatches(candidate)) {
                handle_ = candidate;
                SetupDiDestroyDeviceInfoList(devices);
                return true;
            }
            if(candidate != INVALID_HANDLE_VALUE) {
                CloseHandle(candidate);
            }
        }
        SetupDiDestroyDeviceInfoList(devices);
    }
    // Some older SigmaStar drivers do not expose a usable disk interface
    // path. Retain the vendor tool's drive-letter fallback for that case.
    for(wchar_t letter = L'D'; letter <= L'Z'; ++letter) {
        wchar_t path[] = L"\\\\.\\D:";
        path[4]        = letter;
        auto candidate = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if(candidate == INVALID_HANDLE_VALUE) {
            continue;
        }
        if(inquiryMatches(candidate)) {
            handle_ = candidate;
            return true;
        }
        CloseHandle(candidate);
    }
    return false;
}
void WinScsiDevicePort::execute(const uint8_t *cdb, uint8_t cdbSize, ScsiDataDirection direction, uint8_t *data, uint32_t dataSize, uint32_t timeoutMs) {
    if(handle_ == INVALID_HANDLE_VALUE || !cdb || !cdbSize || cdbSize > 16 || (dataSize && !data)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid SCSI request");
    }
    uint32_t dataBufferSize = dataSize;
    if(dataBufferSize == 0) {
        dataBufferSize = 1;
    }
    const size_t         size = offsetof(ScsiPacket, data) + dataBufferSize;
    std::vector<uint8_t> bytes(size, 0);
    auto                 packet    = reinterpret_cast<ScsiPacket *>(bytes.data());
    packet->spt.Length             = sizeof(SCSI_PASS_THROUGH);
    packet->spt.CdbLength          = cdbSize;
    packet->spt.SenseInfoLength    = sizeof(packet->sense);
    packet->spt.DataIn = SCSI_IOCTL_DATA_OUT;
    if(direction == ScsiDataDirection::In) {
        packet->spt.DataIn = SCSI_IOCTL_DATA_IN;
    }
    packet->spt.DataTransferLength = dataSize;
    packet->spt.TimeOutValue       = (timeoutMs + 999) / 1000;
    packet->spt.DataBufferOffset   = offsetof(ScsiPacket, data);
    packet->spt.SenseInfoOffset    = offsetof(ScsiPacket, sense);
    std::memcpy(packet->spt.Cdb, cdb, cdbSize);
    if(direction == ScsiDataDirection::Out && dataSize) {
        std::memcpy(packet->data, data, dataSize);
    }
    DWORD returned = 0;
    if(!DeviceIoControl(handle_, IOCTL_SCSI_PASS_THROUGH, packet, static_cast<DWORD>(size), packet, static_cast<DWORD>(size), &returned, nullptr)) {
        THROW_IO_EXCEPTION("IOCTL_SCSI_PASS_THROUGH failed");
    }
    if(packet->spt.ScsiStatus != 0) {
        THROW_IO_EXCEPTION("SCSI device returned failure status");
    }
    if(direction == ScsiDataDirection::In && dataSize) {
        std::memcpy(data, packet->data, dataSize);
    }
}
std::shared_ptr<const SourcePortInfo> WinScsiDevicePort::getSourcePortInfo() const {
    return portInfo_;
}
}  // namespace libobsensor

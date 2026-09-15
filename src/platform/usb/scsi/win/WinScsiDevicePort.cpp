// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "WinScsiDevicePort.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
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
#include <exception>
#include <iomanip>
#include <sstream>
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
std::string wideToUtf8(const wchar_t *value) {
    if(!value || !*value) {
        return {};
    }
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if(size <= 1) {
        return {};
    }
    std::string result(static_cast<size_t>(size), '\0');
    if(WideCharToMultiByte(CP_UTF8, 0, value, -1, &result[0], size, nullptr, nullptr) != size) {
        return {};
    }
    result.resize(static_cast<size_t>(size - 1));
    return result;
}
std::string win32ErrorDescription(DWORD error) {
    wchar_t *message = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
                                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    std::string description;
    if(length && message) {
        description = wideToUtf8(message);
        while(!description.empty() && (description.back() == '\r' || description.back() == '\n' || description.back() == ' ')) {
            description.pop_back();
        }
    }
    if(message) {
        LocalFree(message);
    }
    return description;
}
std::string formatWin32Error(DWORD error) {
    auto description = win32ErrorDescription(error);
    return description.empty() ? std::to_string(error) : std::to_string(error) + " (" + description + ")";
}
std::string bytesToHex(const uint8_t *data, size_t size) {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for(size_t i = 0; i < size; ++i) {
        if(i) {
            stream << ' ';
        }
        stream << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return stream.str();
}
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
bool WinScsiDevicePort::inquiryMatches(HANDLE candidate, const std::string &candidatePath) {
    auto saved = handle_;
    handle_    = candidate;
    std::array<uint8_t, 36> data{};
    const uint8_t           cdb[6] = { 0x12, 0, 0, 0, 36, 0 };
    try {
        execute(cdb, 6, ScsiDataDirection::In, data.data(), 36, 2000);
    }
    catch(const std::exception &e) {
        handle_ = saved;
        LOG_WARN("EGO recovery SCSI Inquiry failed for '{}': {}", candidatePath, e.what());
        return false;
    }
    catch(...) {
        handle_ = saved;
        LOG_WARN("EGO recovery SCSI Inquiry failed for '{}' with an unknown exception", candidatePath);
        return false;
    }
    handle_ = saved;
    const bool matches = data[8] == 'G' && data[9] == 'C';
    if(!matches) {
        LOG_DEBUG("EGO recovery SCSI Inquiry signature mismatch for '{}': expected 47 43, received {:02x} {:02x}; inquiry data: {}",
                  candidatePath, static_cast<unsigned int>(data[8]), static_cast<unsigned int>(data[9]), bytesToHex(data.data(), data.size()));
    }
    else {
        LOG_DEBUG("EGO recovery SCSI Inquiry matched for '{}'; inquiry data: {}", candidatePath, bytesToHex(data.data(), data.size()));
    }
    return matches;
}
bool WinScsiDevicePort::openEgoVolume() {
    LOG_DEBUG("Searching for EGO recovery mass-storage volume, expected uid: '{}'", portInfo_->uid);
    HDEVINFO devices = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if(devices != INVALID_HANDLE_VALUE) {
        for(DWORD index = 0;; ++index) {
            SP_DEVICE_INTERFACE_DATA interfaceData{};
            interfaceData.cbSize = sizeof(interfaceData);
            if(!SetupDiEnumDeviceInterfaces(devices, nullptr, &GUID_DEVINTERFACE_DISK, index, &interfaceData)) {
                const auto error = GetLastError();
                if(error != ERROR_NO_MORE_ITEMS) {
                    LOG_WARN("SetupDiEnumDeviceInterfaces failed at index {}: {}", index, formatWin32Error(error));
                }
                break;
            }
            DWORD           required = 0;
            SP_DEVINFO_DATA deviceData{};
            deviceData.cbSize = sizeof(deviceData);
            SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, nullptr, 0, &required, &deviceData);
            if(!required) {
                const auto error = GetLastError();
                LOG_WARN("Failed to query disk interface detail size at index {}: {}", index, formatWin32Error(error));
                continue;
            }
            std::vector<uint8_t> detailBytes(required);
            auto                 detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailBytes.data());
            detail->cbSize              = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
            if(!SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, detail, required, nullptr, &deviceData)) {
                const auto error = GetLastError();
                LOG_WARN("Failed to query disk interface detail at index {}: {}", index, formatWin32Error(error));
                continue;
            }
            std::string usbParent;
            if(!findUsbParent(deviceData.DevInst, usbParent)) {
                continue;
            }
            const auto devicePath = wideToUtf8(detail->DevicePath);
            LOG_DEBUG("Found EGO recovery disk candidate: path='{}', usbParent='{}'", devicePath, usbParent);
            if(!portInfo_->uid.empty() && normalizedInstanceUid(usbParent) != normalizedInstanceUid(portInfo_->uid)) {
                LOG_DEBUG("Skipping EGO recovery disk candidate because uid does not match: expected='{}', actual='{}'", portInfo_->uid, usbParent);
                continue;
            }
            auto candidate =
                CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if(candidate == INVALID_HANDLE_VALUE) {
                const auto error = GetLastError();
                LOG_WARN("Failed to open EGO recovery disk candidate: path='{}', usbParent='{}', error={}", devicePath, usbParent,
                         formatWin32Error(error));
                continue;
            }
            if(inquiryMatches(candidate, devicePath)) {
                handle_ = candidate;
                LOG_INFO("Opened EGO recovery mass-storage volume: path='{}', usbParent='{}'", devicePath, usbParent);
                SetupDiDestroyDeviceInfoList(devices);
                return true;
            }
            CloseHandle(candidate);
        }
        SetupDiDestroyDeviceInfoList(devices);
    }
    else {
        const auto error = GetLastError();
        LOG_WARN("SetupDiGetClassDevsW failed while enumerating EGO recovery disks: {}", formatWin32Error(error));
    }
    // Some older SigmaStar drivers do not expose a usable disk interface
    // path. Retain the vendor tool's drive-letter fallback for that case.
    for(wchar_t letter = L'D'; letter <= L'Z'; ++letter) {
        wchar_t path[] = L"\\\\.\\D:";
        path[4]        = letter;
        auto candidate = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if(candidate == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            LOG_DEBUG("Failed to open EGO recovery drive-letter candidate '{}': {}", wideToUtf8(path), formatWin32Error(error));
            continue;
        }
        LOG_DEBUG("Opened EGO recovery drive-letter candidate '{}'; checking SCSI Inquiry", wideToUtf8(path));
        const auto candidatePath = wideToUtf8(path);
        if(inquiryMatches(candidate, candidatePath)) {
            handle_ = candidate;
            LOG_INFO("Opened EGO recovery mass-storage volume via drive-letter fallback: '{}'", candidatePath);
            return true;
        }
        CloseHandle(candidate);
    }
    LOG_ERROR("No usable EGO recovery mass-storage volume was found, expected uid: '{}'", portInfo_->uid);
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
        const auto error = GetLastError();
        LOG_ERROR("IOCTL_SCSI_PASS_THROUGH failed: win32Error={}, opcode=0x{:02x}, cdb={}, direction={}, dataSize={}, timeoutMs={}",
                  formatWin32Error(error), static_cast<unsigned int>(cdb[0]), bytesToHex(cdb, cdbSize), static_cast<int>(direction), dataSize,
                  timeoutMs);
        THROW_IO_EXCEPTION("IOCTL_SCSI_PASS_THROUGH failed");
    }
    if(packet->spt.ScsiStatus != 0) {
        LOG_ERROR("SCSI device returned failure status: status=0x{:02x}, opcode=0x{:02x}, cdb={}, sense={}",
                  static_cast<unsigned int>(packet->spt.ScsiStatus), static_cast<unsigned int>(cdb[0]), bytesToHex(cdb, cdbSize),
                  bytesToHex(packet->sense, sizeof(packet->sense)));
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

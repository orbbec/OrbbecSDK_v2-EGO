// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "WinUsbPal.hpp"

#if(_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <dbt.h>
#include <mfapi.h>
#include <SetupAPI.h>
#include <ks.h>
#include <initguid.h>
#include <devpkey.h>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "SetupAPI.lib")

#include <cctype>  //Std::tolower
#include <chrono>
#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

#include "usb/uvc/WmfUvcDevicePort.hpp"
#include "usb/hid/HidDevicePort.hpp"
#include "usb/uac/WasapiUacDevicePort.hpp"
#include "usb/uac/WasapiUacPlaybackPort.hpp"
#include "usb/vendor/VendorUsbDevicePort.hpp"
#include "usb/scsi/win/WinScsiDevicePort.hpp"

#include "logger/Logger.hpp"
#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

constexpr GUID     GUID_DEVINTERFACE_USB_DEVICE = { 0xA5DCBF10, 0x6530, 0x11D2, { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED } };
constexpr GUID     OB_GUID_DEVINTERFACE_HID     = { 0x4d1e55b2, 0xf16f, 0x11cf, { 0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };
constexpr uint16_t PID_BOOTLOADER_UVC           = 0x0501;
constexpr uint16_t EGO_BOOT_DEVICE_VID           = 0x1B20;
constexpr uint16_t PID_EGO_BOOT_ROM              = 0x0300;
constexpr uint16_t PID_EGO_BOOT_UBOOT            = 0x0301;
constexpr uint16_t EGOPRO_RK_BOOT_DEVICE_VID     = 0x2207;
constexpr uint16_t PID_EGOPRO_RK_BOOT_LOADER     = 0x350E;
constexpr uint16_t PID_UMI_RK_BOOT_LOADER        = 0x110F;
namespace libobsensor {

namespace {

const auto EGO_DEPTH_HID_INTERFACE_TTL = std::chrono::seconds(60);

std::wstring utf8ToWide(const std::string &value) {
    if(value.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if(size <= 1) {
        return {};
    }
    std::wstring result(static_cast<size_t>(size), L'\0');
    if(MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], size) != size) {
        return {};
    }
    result.resize(static_cast<size_t>(size - 1));
    return result;
}

std::string guidToKey(const GUID &guid) {
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(8) << guid.Data1 << '-' << std::setw(4) << guid.Data2 << '-' << std::setw(4) << guid.Data3 << '-'
       << std::setw(2) << static_cast<uint32_t>(guid.Data4[0]) << std::setw(2) << static_cast<uint32_t>(guid.Data4[1]) << '-';
    for(size_t i = 2; i < sizeof(guid.Data4); ++i) {
        ss << std::setw(2) << static_cast<uint32_t>(guid.Data4[i]);
    }
    return utils::string::toUpper(ss.str());
}

bool getDeviceContainerKey(const std::string &interfacePath, std::string &containerKey) {
    const auto widePath = utf8ToWide(interfacePath);
    if(widePath.empty()) {
        return false;
    }

    auto deviceInfoSet = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if(deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData{};
    interfaceData.cbSize = sizeof(interfaceData);
    if(!SetupDiOpenDeviceInterfaceW(deviceInfoSet, widePath.c_str(), 0, &interfaceData)) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    DWORD requiredSize = 0;
    SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, nullptr, 0, &requiredSize, nullptr);
    if(requiredSize == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    std::vector<uint8_t> detailBuffer(requiredSize);
    auto detailData    = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(detailBuffer.data());
    detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    SP_DEVINFO_DATA deviceData{};
    deviceData.cbSize = sizeof(deviceData);
    if(!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, detailData, requiredSize, nullptr, &deviceData)) {
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return false;
    }

    GUID        containerId{};
    DEVPROPTYPE propertyType = 0;
    DWORD       propertySize = sizeof(containerId);
    const auto success = SetupDiGetDevicePropertyW(deviceInfoSet, &deviceData, &DEVPKEY_Device_ContainerId, &propertyType,
                                                    reinterpret_cast<PBYTE>(&containerId), propertySize, &propertySize, 0);
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    if(!success || propertyType != DEVPROP_TYPE_GUID || propertySize != sizeof(containerId) || IsEqualGUID(containerId, GUID_NULL)) {
        return false;
    }
    containerKey = guidToKey(containerId) + ":MI08";
    return true;
}

}  // namespace

class WinUsbDeviceWatcher : public IDeviceWatcher {
public:
    WinUsbDeviceWatcher(const WinUsbPal *backend);
    ~WinUsbDeviceWatcher() noexcept override;

    void start(deviceChangedCallback callback) override;
    void stop() override;

private:
    void                    run();
    static LRESULT CALLBACK onWinEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static bool             registerDeviceInterfaceToHwnd(HWND hWnd);

private:
    std::thread eventThread_;
    std::mutex  mutex_;
    struct extra_data {
        const WinUsbPal      *backend_;
        deviceChangedCallback callback_;
        bool                  stopped_;
        HWND                  hWnd;
        HDEVNOTIFY            hDevNotifyHW;
        HDEVNOTIFY            hDevNotifyUVC;
        HDEVNOTIFY            hDevNotifySensor;
        HDEVNOTIFY            hDevNotifyHID;
        HDEVNOTIFY            hDevNotifyUSB;
        HDEVNOTIFY            hDevNotifyOpenNI;
    } extraData_;
};

std::vector<std::string> stringSplit(const std::string &string, char separator) {
    std::vector<std::string> tokens;
    std::string::size_type   i1 = 0;
    while(true) {
        auto i2 = string.find(separator, i1);
        if(i2 == std::string::npos) {
            tokens.push_back(string.substr(i1));
            return tokens;
        }
        tokens.push_back(string.substr(i1, i2 - i1));
        i1 = i2 + 1;
    }
}

bool parseSymbolicLink(const std::string &symbolicLink, uint16_t &vid, uint16_t &pid, uint16_t &mi, std::string &uid, std::string &device_guid) {
    auto lowerStr = utils::string::toLower(symbolicLink);
    auto tokens   = stringSplit(lowerStr, '#');
    if(tokens.empty() || (tokens[0] != R"(\\?\usb)" && tokens[0] != R"(\\?\hid)"))
        return false;  // Not a USB device
    if(tokens.size() < 3) {
        return false;
    }

    auto ids = stringSplit(tokens[1], '&');
    if(ids[0].size() != 8 || ids[0].substr(0, 4) != "vid_" || !(std::istringstream(ids[0].substr(4, 4)) >> std::hex >> vid)) {
        return false;
    }
    if(ids[1].size() != 8 || ids[1].substr(0, 4) != "pid_" || !(std::istringstream(ids[1].substr(4, 4)) >> std::hex >> pid)) {
        return false;
    }
    if(ids.size() > 2 && (ids[2].size() != 5 || ids[2].substr(0, 3) != "mi_" || !(std::istringstream(ids[2].substr(3, 2)) >> mi))) {
        return false;
    }
    ids = stringSplit(tokens[2], '&');
    if(ids.empty()) {
        return false;
    }

    if(ids.size() > 2)
        uid = ids[1];
    else
        uid = "";

    if(tokens.size() >= 3)
        device_guid = tokens[3];

    return true;
}

std::shared_ptr<IPal> createUsbPal() {
    return std::make_shared<WinUsbPal>();
}

WinUsbPal::WinUsbPal() {
    LOG_DEBUG("WinUsbPal init ...");
    // when using COINIT_APARTMENTTHREADED, calling _pISensor->SetEventSink(NULL) to stop sensor can take several seconds
    (void)CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);

    usbEnumerator_ = IUsbEnumerator::getInstance();
    LOG_DEBUG("WinUsbPal created!");
}

WinUsbPal::~WinUsbPal() noexcept {
    TRY_EXECUTE({
        MFShutdown();
        CoUninitialize();
    });
    LOG_DEBUG("WinUsbPal destroyed!");
}

void WinUsbPal::markUvcPortsDisconnected(uint16_t vid, uint16_t pid, const std::string &uid, const std::string &symbolicLink) const {
    auto                                           removedUrl = utils::string::toUpper(symbolicLink);
    std::vector<std::shared_ptr<WmfUvcDevicePort>> disconnectedPorts;

    {
        std::lock_guard<std::mutex> lock(sourcePortMapMutex_);
        for(auto it = sourcePortMap_.begin(); it != sourcePortMap_.end();) {
            auto port = it->second.lock();
            if(!port) {
                it = sourcePortMap_.erase(it);
                continue;
            }

            auto portInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(it->first);
            if(portInfo && portInfo->portType == SOURCE_PORT_USB_UVC && portInfo->vid == vid && portInfo->pid == pid) {
                auto       portUrl    = utils::string::toUpper(portInfo->url);
                const bool urlMatched = !portUrl.empty() && portUrl == removedUrl;
                const bool uidMatched = !uid.empty() && portInfo->uid == uid;
                if(urlMatched || uidMatched) {
                    auto uvcPort = std::dynamic_pointer_cast<WmfUvcDevicePort>(port);
                    if(uvcPort) {
                        disconnectedPorts.push_back(std::move(uvcPort));
                    }
                }
            }
            ++it;
        }
    }

    for(const auto &port: disconnectedPorts) {
        port->markDisconnected();
    }
}
std::shared_ptr<ISourcePort> WinUsbPal::getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) {
    std::unique_lock<std::mutex> lock(sourcePortMapMutex_);
    std::shared_ptr<ISourcePort> port;

    // clear expired weak_ptr
    for(auto it = sourcePortMap_.begin(); it != sourcePortMap_.end();) {
        if(it->second.expired()) {
            it = sourcePortMap_.erase(it);
        }
        else {
            ++it;
        }
    }

    // check if the port already exists in the map
    for(auto it = sourcePortMap_.begin(); it != sourcePortMap_.end();) {
        if(it->first == portInfo) {
            port = it->second.lock();
            if(port != nullptr && !port->isDisconnected()) {
                return port;
            }
            it = sourcePortMap_.erase(it);
            break;
        }
        else {
            ++it;
        }
    }

    switch(portInfo->portType) {
    case SOURCE_PORT_USB_VENDOR: {
        auto usbDev = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            THROW_DEVICE_UNAVAILABLE_EXCEPTION("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<VendorUsbDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    }
    case SOURCE_PORT_USB_UVC:
        port = std::make_shared<WmfUvcDevicePort>(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_USB_UAC:
        // Windows captures the UAC microphone via WASAPI, paired to the device by USB VID/PID.
        port = std::make_shared<WasapiUacDevicePort>(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_USB_UAC_PLAYBACK:
        port = std::make_shared<WasapiUacPlaybackPort>(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    case SOURCE_PORT_USB_HID: {
        auto usbPortInfo = std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo);
        auto usbDev      = usbEnumerator_->openUsbDevice(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo)->url);
        if(usbDev == nullptr) {
            THROW_DEVICE_DISCONNECTED_EXCEPTION("usbEnumerator openUsbDevice failed!");
        }
        port = std::make_shared<HidDevicePort>(usbDev, std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));

        break;
    }
    case SOURCE_PORT_USB_MASS_STORAGE:
        port = std::make_shared<WinScsiDevicePort>(std::dynamic_pointer_cast<const USBSourcePortInfo>(portInfo));
        break;
    default:
        THROW_INVALID_PARAM_EXCEPTION("unsupported source port type!");
    }

    sourcePortMap_.insert(std::make_pair(portInfo, port));
    return port;
}

std::shared_ptr<IDeviceWatcher> WinUsbPal::createDeviceWatcher() const {
    LOG_DEBUG("Create WinUsbDeviceWatcher!");
    return std::make_shared<WinUsbDeviceWatcher>(this);
}

void WinUsbPal::pruneExpiredEgoDepthHidInterfaces(std::chrono::steady_clock::time_point now) const {
    for(auto it = egoDepthHidInterfaces_.begin(); it != egoDepthHidInterfaces_.end();) {
        if(now - it->second.arrivalTime > EGO_DEPTH_HID_INTERFACE_TTL) {
            it = egoDepthHidInterfaces_.erase(it);
        }
        else {
            ++it;
        }
    }
}

void WinUsbPal::cacheEgoDepthHidInterface(const std::string &symbolicLink) const {
    std::string containerKey;
    if(!getDeviceContainerKey(symbolicLink, containerKey)) {
        LOG_WARN("Failed to get Windows container id for EgoDepth HID MI_08 arrival");
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(egoDepthHidInterfaceMutex_);
        pruneExpiredEgoDepthHidInterfaces(now);
        egoDepthHidInterfaces_[containerKey] = EgoDepthHidInterfaceEntry{ utils::string::toUpper(symbolicLink), now };
    }
    LOG_DEBUG("Cached EgoDepth HID MI_08 interface by container id: {}", containerKey);
}

void WinUsbPal::removeEgoDepthHidInterface(const std::string &symbolicLink) const {
    const auto normalizedSymbolicLink = utils::string::toUpper(symbolicLink);
    std::lock_guard<std::mutex> lock(egoDepthHidInterfaceMutex_);
    for(auto it = egoDepthHidInterfaces_.begin(); it != egoDepthHidInterfaces_.end();) {
        if(utils::string::toUpper(it->second.symbolicLink) == normalizedSymbolicLink) {
            it = egoDepthHidInterfaces_.erase(it);
        }
        else {
            ++it;
        }
    }
}

bool WinUsbPal::findEgoDepthHidInterface(const std::string &usbDevicePath, std::string &symbolicLink) const {
    std::string containerKey;
    if(!getDeviceContainerKey(usbDevicePath, containerKey)) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(egoDepthHidInterfaceMutex_);
        pruneExpiredEgoDepthHidInterfaces(now);
        const auto it = egoDepthHidInterfaces_.find(containerKey);
        if(it == egoDepthHidInterfaces_.end()) {
            return false;
        }
        symbolicLink = it->second.symbolicLink;
    }
    LOG_DEBUG("Matched EgoDepth HID MI_08 fallback by container id: {}", containerKey);
    return true;
}

SourcePortInfoList WinUsbPal::querySourcePortInfos() {
    SourcePortInfoList portInfoList;

    auto action = [&](const UsbInterfaceInfo &info, IMFActivate *) {
        UsbSpec     usbSpec = usb_undefined;
        std::string hubId, serial, url;
        if(!getUsbDescriptors(info.vid, info.pid, info.uid, hubId, usbSpec, serial, url)) {
            // Some composite UVC devices can be visible to Media Foundation while libusb cannot open the
            // device node to read descriptors. Keep the WMF UVC port so device matching can still proceed.
            url = info.uid.empty() ? info.infUrl : info.uid;
            LOG_WARN("Failed to read USB descriptors for UVC device, use WMF interface info as fallback. vid=0x{:04x}, pid=0x{:04x}, interface index={}",
                     info.vid, info.pid, static_cast<uint32_t>(info.infIndex));
        }

        auto portInfo      = std::make_shared<USBSourcePortInfo>();
        portInfo->portType = SOURCE_PORT_USB_UVC;
        portInfo->url      = url;
        portInfo->uid      = info.uid;
        portInfo->vid      = info.vid;
        portInfo->pid      = info.pid;
        portInfo->serial   = serial;
        portInfo->connSpec = usbSpecToString(static_cast<UsbSpec>(usbSpec));
        portInfo->infUrl   = info.infUrl;
        portInfo->infIndex = info.infIndex;
        portInfo->infName  = info.infName;
        portInfo->hubId    = hubId;

        portInfoList.push_back(portInfo);
    };

    WmfUvcDevicePort::foreachUvcDevice(action);

    const auto &usbInfoList = usbEnumerator_->queryUsbInterfaces();
    for(const auto &info: usbInfoList) {
        if(isSupportedDevice(info.vid, info.pid)
           && (info.cls == OB_USB_CLASS_HID || info.cls == OB_USB_CLASS_VENDOR_SPECIFIC || info.cls == OB_USB_CLASS_MASS_STORAGE)) {
            // 1. Filter non orbbec devices 2. Filter uvc class
            auto portInfo      = std::make_shared<USBSourcePortInfo>();
            portInfo->portType = cvtUsbClassToPortType(info.cls);
            portInfo->url      = info.url;
            portInfo->uid      = info.uid;
            portInfo->vid      = info.vid;
            portInfo->pid      = info.pid;
            portInfo->serial   = info.serial;
            portInfo->connSpec = usbSpecToString(static_cast<UsbSpec>(info.conn_spec));
            portInfo->infUrl   = info.infUrl;
            portInfo->infIndex = info.infIndex;
            portInfo->infName  = info.infName;
            portInfo->hubId    = info.hubId;

            if(portInfo->infUrl.empty() && info.cls == OB_USB_CLASS_HID && info.infIndex == 8 && isEgoDepthDevice(info.vid, info.pid)) {
                std::string hidSymbolicLink;
                if(findEgoDepthHidInterface(info.url, hidSymbolicLink)) {
                    portInfo->infUrl = hidSymbolicLink;
                    LOG_INFO("Use Windows HID arrival path as EgoDepth MI_08 interface-url fallback. vid=0x{:04X}, pid=0x{:04X}", info.vid, info.pid);
                }
            }

            if(portInfo->infUrl.empty()) {
                LOG_WARN("Found a usb port without interface url! drop it!(device url={} interface index={})", info.url,
                         static_cast<uint32_t>(portInfo->infIndex));
                break;
            }
            portInfoList.push_back(portInfo);
        }
    }
    return portInfoList;
}

WinUsbDeviceWatcher::WinUsbDeviceWatcher(const WinUsbPal *backend) {
    extraData_.backend_         = backend;
    extraData_.stopped_         = true;
    extraData_.callback_        = nullptr;
    extraData_.hWnd             = NULL;
    extraData_.hDevNotifyHW     = NULL;
    extraData_.hDevNotifyUVC    = NULL;
    extraData_.hDevNotifySensor = NULL;
    extraData_.hDevNotifyHID    = NULL;
    extraData_.hDevNotifyUSB    = NULL;
    extraData_.hDevNotifyOpenNI = NULL;
}

WinUsbDeviceWatcher::~WinUsbDeviceWatcher() noexcept {
    WinUsbDeviceWatcher::stop();
}

void WinUsbDeviceWatcher::start(deviceChangedCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!extraData_.stopped_)
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("Cannot start a running device_watcher");
    extraData_.stopped_  = false;
    extraData_.callback_ = std::move(callback);
    eventThread_         = std::thread([this]() { run(); });
}

void WinUsbDeviceWatcher::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!extraData_.stopped_) {
        extraData_.stopped_ = true;
        if(eventThread_.joinable())
            eventThread_.join();
    }
}

void WinUsbDeviceWatcher::run() {
    WNDCLASS windowClass      = {};
    LPCWSTR  winClassName     = TEXT("OBMINWINAPP");
    windowClass.lpfnWndProc   = &onWinEvent;
    windowClass.lpszClassName = winClassName;
    UnregisterClass(winClassName, nullptr);

    if(!RegisterClass(&windowClass))
        LOG_WARN("RegisterClass failed.");

    extraData_.hWnd = CreateWindow(winClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, &extraData_);
    if(!extraData_.hWnd)
        THROW_PAL_EXCEPTION("CreateWindow failed", OB_ERROR_UNKNOWN);

    MSG msg;

    while(!extraData_.stopped_) {
        if(PeekMessage(&msg, extraData_.hWnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else  // Yield CPU resources, as this is required for connect/disconnect events only
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // UnregisterDeviceNotification(extraData_.hDevNotifyHW);
    UnregisterDeviceNotification(extraData_.hDevNotifyUVC);
    UnregisterDeviceNotification(extraData_.hDevNotifySensor);
    UnregisterDeviceNotification(extraData_.hDevNotifyHID);
    UnregisterDeviceNotification(extraData_.hDevNotifyUSB);
    UnregisterDeviceNotification(extraData_.hDevNotifyOpenNI);
    DestroyWindow(extraData_.hWnd);
}

std::string wideCharToUTF8(const wchar_t *wStr) {
    int size = WideCharToMultiByte(CP_UTF8, 0, wStr, -1, nullptr, 0, nullptr, nullptr);
    if(size <= 0) {
        return "";
    }
    std::string result(size - 1, '\0');
    if(WideCharToMultiByte(CP_UTF8, 0, wStr, -1, &result[0], size, nullptr, nullptr) != size) {
        return "";
    }
    return result;
}

LRESULT CALLBACK WinUsbDeviceWatcher::onWinEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT lRet = 1;
    switch(message) {
    case WM_CREATE:
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams));
        if(!registerDeviceInterfaceToHwnd(hWnd)) {
            auto watcherExtraData      = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
            watcherExtraData->stopped_ = true;
        }
        break;

    case WM_QUIT: {
        auto watcherExtraData      = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
        watcherExtraData->stopped_ = true;
        break;
    }
    case WM_DEVICECHANGE: {
        if(wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
            uint16_t    vid = 0;
            uint16_t    pid = 0;
            uint16_t    mi  = 0xFFFF;
            std::string uid;
            std::string device_guid;
            auto        devIntf      = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE>(lParam);
            std::string symbolicLink = wideCharToUTF8(devIntf->dbcc_name);
            if(parseSymbolicLink(symbolicLink, vid, pid, mi, uid, device_guid)) {
                if(isSupportedDevice(vid, pid)) {
                    auto watcherExtraData = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
                    const auto nativeSymbolicLink = symbolicLink;
                    symbolicLink                 = utils::string::toUpper(symbolicLink);
                    if(wParam == DBT_DEVICEARRIVAL) {
                        LOG_DEBUG("Device arrival event occurred! symbolicLink={}", symbolicLink);
                        if(IsEqualGUID(devIntf->dbcc_classguid, OB_GUID_DEVINTERFACE_HID) && mi == 8 && isEgoDepthDevice(vid, pid)) {
                            auto winPal = dynamic_cast<const WinUsbPal *>(watcherExtraData->backend_);
                            if(winPal != nullptr) {
                                winPal->cacheEgoDepthHidInterface(nativeSymbolicLink);
                            }
                        }
                        const bool isUsbDeviceInterface  = (devIntf->dbcc_classguid == GUID_DEVINTERFACE_USB_DEVICE);
                        const bool isUvcBootloader       = (ORBBEC_DEVICE_VID == vid) && (PID_BOOTLOADER_UVC == pid);
                        const bool isEgoBootDevice       = (vid == EGO_BOOT_DEVICE_VID) && (pid == PID_EGO_BOOT_ROM || pid == PID_EGO_BOOT_UBOOT);
                        const bool isEgoProRkBootDevice = (vid == EGOPRO_RK_BOOT_DEVICE_VID) && (pid == PID_EGOPRO_RK_BOOT_LOADER || pid == PID_UMI_RK_BOOT_LOADER);

                        // Generic USB-device notifications are normally suppressed because a
                        // UVC/HID device also generates its own interface notifications. EGO
                        // boot devices expose only this generic USB mass-storage notification,
                        // so allow only known boot/recovery PIDs through. RockUSB presents
                        // the same generic interface and must also be forwarded.
                        if(!isUsbDeviceInterface || isUvcBootloader || isEgoBootDevice || isEgoProRkBootDevice) {
                            (void)watcherExtraData->callback_(OB_DEVICE_ARRIVAL, symbolicLink);
                        }
                    }
                    else if(wParam == DBT_DEVICEREMOVECOMPLETE) {
                        LOG_DEBUG("Device removed event occurred! symbolicLink={}", symbolicLink);
                        if(IsEqualGUID(devIntf->dbcc_classguid, OB_GUID_DEVINTERFACE_HID) && mi == 8 && isEgoDepthDevice(vid, pid)) {
                            auto winPal = dynamic_cast<const WinUsbPal *>(watcherExtraData->backend_);
                            if(winPal != nullptr) {
                                winPal->removeEgoDepthHidInterface(nativeSymbolicLink);
                            }
                        }
                        if(devIntf->dbcc_classguid == GUID_DEVINTERFACE_USB_DEVICE) {
                            watcherExtraData->backend_->markUvcPortsDisconnected(vid, pid, uid, symbolicLink);
                            (void)watcherExtraData->callback_(OB_DEVICE_REMOVED, symbolicLink);
                        }
                    }
                }
            }
        }
        break;
    }

    default:
        // Send all other messages on to the default windows handler.
        lRet = DefWindowProc(hWnd, message, wParam, lParam);
        break;
    }

    return lRet;
}

bool WinUsbDeviceWatcher::registerDeviceInterfaceToHwnd(HWND hWnd) {
    auto data = reinterpret_cast<extra_data *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    //===========================register HWmonitor events==============================
    const GUID                    classGuid = { 0x175695cd, 0x30d9, 0x4f87, 0x8b, 0xe3, 0x5a, 0x82, 0x70, 0xf4, 0x9a, 0x31 };
    DEV_BROADCAST_DEVICEINTERFACE devBroadcastDeviceInterface;
    devBroadcastDeviceInterface.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devBroadcastDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcastDeviceInterface.dbcc_classguid  = classGuid;
    devBroadcastDeviceInterface.dbcc_reserved   = 0;

    data->hDevNotifyHW = RegisterDeviceNotification(hWnd, &devBroadcastDeviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyHW == nullptr) {
        LOG_WARN("Register HW events Failed!");
        return false;
    }

    //===========================register UVC events==============================
    DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
    di.dbcc_size                     = sizeof(di);
    di.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    di.dbcc_classguid                = KSCATEGORY_CAPTURE;

    data->hDevNotifyUVC = RegisterDeviceNotification(hWnd, &di, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyUVC == nullptr) {
        LOG_WARN("Register UVC events Failed!");
        return false;
    }

    //===========================register UVC sensor camera events==============================
    DEV_BROADCAST_DEVICEINTERFACE di_sensor = { 0 };
    di_sensor.dbcc_size                     = sizeof(di_sensor);
    di_sensor.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    di_sensor.dbcc_classguid                = KSCATEGORY_SENSOR_CAMERA;

    data->hDevNotifySensor = RegisterDeviceNotification(hWnd, &di_sensor, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifySensor == nullptr) {
        LOG_WARN("Register UVC events Failed!");
        return false;
    }

    //===========================register HID sensor camera events==============================
    DEV_BROADCAST_DEVICEINTERFACE hid_sensor = { 0 };
    hid_sensor.dbcc_size                     = sizeof(hid_sensor);
    hid_sensor.dbcc_devicetype               = DBT_DEVTYP_DEVICEINTERFACE;
    hid_sensor.dbcc_classguid                = OB_GUID_DEVINTERFACE_HID;

    data->hDevNotifyHID = RegisterDeviceNotification(hWnd, &hid_sensor, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyHID == nullptr) {
        LOG_WARN("Register hid events Failed!");
        return false;
    }

    //===========================register usb device events==============================
    DEV_BROADCAST_DEVICEINTERFACE devBroadcastUsbDeviceInterface;
    devBroadcastUsbDeviceInterface.dbcc_size       = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devBroadcastUsbDeviceInterface.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devBroadcastUsbDeviceInterface.dbcc_classguid  = GUID_DEVINTERFACE_USB_DEVICE;
    devBroadcastUsbDeviceInterface.dbcc_reserved   = 0;

    data->hDevNotifyUSB = RegisterDeviceNotification(hWnd, &devBroadcastUsbDeviceInterface, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(data->hDevNotifyUSB == nullptr) {
        LOG_WARN("Register HW events Failed!");
        return false;
    }

    return true;
}

}  // namespace libobsensor

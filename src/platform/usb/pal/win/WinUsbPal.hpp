// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IPal.hpp"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>

#include "usb/enumerator/IUsbEnumerator.hpp"

namespace libobsensor {

class WinUsbPal : public IPal, public std::enable_shared_from_this<WinUsbPal> {
public:
    WinUsbPal();
    virtual ~WinUsbPal() noexcept override;

    std::shared_ptr<ISourcePort>    getSourcePort(std::shared_ptr<const SourcePortInfo> portInfo) override;
    std::shared_ptr<IDeviceWatcher> createDeviceWatcher() const override;
    SourcePortInfoList              querySourcePortInfos() override;
    void                            markUvcPortsDisconnected(uint16_t vid, uint16_t pid, const std::string &uid, const std::string &symbolicLink) const;

private:
    friend class WinUsbDeviceWatcher;

    struct EgoDepthHidInterfaceEntry {
        std::string                           symbolicLink;
        std::chrono::steady_clock::time_point arrivalTime;
    };

    void cacheEgoDepthHidInterface(const std::string &symbolicLink) const;
    void removeEgoDepthHidInterface(const std::string &symbolicLink) const;
    bool findEgoDepthHidInterface(const std::string &usbDevicePath, std::string &symbolicLink) const;
    void pruneExpiredEgoDepthHidInterfaces(std::chrono::steady_clock::time_point now) const;

    std::shared_ptr<IUsbEnumerator> usbEnumerator_;

private:
    mutable std::mutex                                                          sourcePortMapMutex_;
    mutable std::map<std::shared_ptr<const SourcePortInfo>, std::weak_ptr<ISourcePort>> sourcePortMap_;
    mutable std::mutex                                                           egoDepthHidInterfaceMutex_;
    mutable std::unordered_map<std::string, EgoDepthHidInterfaceEntry>          egoDepthHidInterfaces_;
};

}  // namespace libobsensor

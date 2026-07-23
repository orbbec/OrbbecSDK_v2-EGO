// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once

#include "usb/scsi/ScsiDevicePort.hpp"
#include "usb/enumerator/IUsbEnumerator.hpp"

namespace libobsensor {
class LinuxScsiDevicePort : public IScsiDevicePort {
public:
    LinuxScsiDevicePort(const std::shared_ptr<IUsbDevice> &usbDevice, std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~LinuxScsiDevicePort() noexcept override;
    void execute(const uint8_t *cdb, uint8_t cdbSize, ScsiDataDirection direction, uint8_t *data, uint32_t dataSize, uint32_t timeoutMs) override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    std::shared_ptr<IUsbDevice>              usbDevice_;
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    uint8_t                                  endpointIn_ = 0, endpointOut_ = 0, interface_ = 0;
    bool                                     detachedKernelDriver_ = false;
    uint32_t                                 tag_                  = 1;
};
}  // namespace libobsensor

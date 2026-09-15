// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "usb/scsi/ScsiDevicePort.hpp"
#include <Windows.h>
#include <string>
namespace libobsensor {
class WinScsiDevicePort : public IScsiDevicePort {
public:
    explicit WinScsiDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~WinScsiDevicePort() noexcept override;
    void                                  execute(const uint8_t *, uint8_t, ScsiDataDirection, uint8_t *, uint32_t, uint32_t) override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    bool                                     openEgoVolume();
    bool                                     inquiryMatches(HANDLE handle, const std::string &candidatePath);
    std::shared_ptr<const USBSourcePortInfo> portInfo_;
    HANDLE                                   handle_ = INVALID_HANDLE_VALUE;
};
}  // namespace libobsensor

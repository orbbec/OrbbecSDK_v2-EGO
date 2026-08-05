// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOBootDevice.hpp"
#include "DevicePids.hpp"
#include "exception/ObException.hpp"
namespace libobsensor {
EGOBootDevice::EGOBootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}
EGOBootDevice::~EGOBootDevice() noexcept = default;
void EGOBootDevice::init() {
    auto ports = enumInfo_->getSourcePortInfoList();
    if(ports.empty()) {
        THROW_DEVICE_UNAVAILABLE_EXCEPTION("Ego boot device has no source port");
    }
    auto info = std::dynamic_pointer_cast<const USBSourcePortInfo>(ports.front());
    if(!info || info->portType != SOURCE_PORT_USB_MASS_STORAGE) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid Ego boot source port");
    }
    // DeviceBase creates a separate public DeviceInfo, so retain the
    // enumeration-stage name instead of collapsing ROM and U-Boot to one.
    deviceInfo_->name_           = enumInfo_->getPid() == EGO_BOOT_ROM_PID ? "Ego Boot ROM Device" : "Ego Boot U-Boot Device";
    deviceInfo_->fullName_       = deviceInfo_->name_;
    deviceInfo_->connectionType_ = "USB";
    updater_.reset(new EGOBinFirmwareUpdater(info, static_cast<uint16_t>(enumInfo_->getPid())));
}
void EGOBootDevice::updateFirmware(const std::vector<uint8_t> &fw, DeviceFwUpdateCallback cb, bool async) {
    if(fw.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("Ego firmware data is empty");
    }
    if(isFirmwareUpdating()) {
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("Ego firmware update is already running");
    }
    setFirmwareUpdateState(true);
    auto wrapped = [this, cb](OBFwUpdateState s, const char *m, uint8_t p) {
        if(s == STAT_DONE || s < 0) {
            setFirmwareUpdateState(false);
        }
        if(cb) {
            cb(s, m, p);
        }
    };
    try {
        updater_->update(fw, wrapped, async);
    }
    catch(...) {
        setFirmwareUpdateState(false);
        throw;
    }
}
}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EgoProRkBootDevice.hpp"
#include "common/DevicePids.hpp"
#include "exception/ObException.hpp"
namespace libobsensor {
EgoProRkBootDevice::EgoProRkBootDevice(const std::shared_ptr<const IDeviceEnumInfo> &info) : DeviceBase(info) {
    init();
}
EgoProRkBootDevice::~EgoProRkBootDevice() noexcept = default;
void EgoProRkBootDevice::init() {
    auto ports = enumInfo_->getSourcePortInfoList();
    if(ports.empty()) {
        THROW_DEVICE_UNAVAILABLE_EXCEPTION("EGOPro boot device has no source port");
    }
    auto info = std::dynamic_pointer_cast<const USBSourcePortInfo>(ports.front());
    if(!info || info->portType != SOURCE_PORT_USB_VENDOR) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGOPro boot source port");
    }
    deviceInfo_->name_           = "EGOPro Boot Device";
    deviceInfo_->fullName_       = deviceInfo_->name_;
    deviceInfo_->connectionType_ = "USB";
    updater_.reset(new EgoProRkFirmwareUpdater(info, static_cast<uint16_t>(enumInfo_->getPid())));
}
void EgoProRkBootDevice::updateFirmware(const std::vector<uint8_t> &fw, DeviceFwUpdateCallback cb, bool async) {
    (void)fw;
    (void)cb;
    (void)async;
    THROW_INVALID_PARAM_EXCEPTION("Firmware update from data is unsupported; use the file-path firmware update API");
}

void EgoProRkBootDevice::updateFirmwareFromPath(const std::string &path, DeviceFwUpdateCallback cb, bool async) {
    if(path.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("EGOPro firmware path is empty");
    }
    if(isFirmwareUpdating()) {
        THROW_WRONG_API_CALL_SEQUENCE_EXCEPTION("EGOPro firmware update is already running");
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
        updater_->updateFromPath(path, wrapped, async);
    }
    catch(...) {
        setFirmwareUpdateState(false);
        throw;
    }
}
}  // namespace libobsensor

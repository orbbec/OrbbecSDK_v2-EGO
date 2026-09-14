// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EgoDeviceMonitor.hpp"

#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

namespace libobsensor {

OBDeviceState EgoDeviceMonitor::getCurrentDeviceState() const {
    LOG_DEBUG("Generic device-state monitoring is disabled for this device");
    return 0;
}

int EgoDeviceMonitor::registerStateChangedCallback(DeviceStateChangedCallback callback) {
    utils::unusedVar(callback);
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Generic device-state monitoring is disabled for this device");
    return -1;
}

void EgoDeviceMonitor::unregisterStateChangedCallback(int callbackId) {
    utils::unusedVar(callbackId);
}

void EgoDeviceMonitor::enableHeartbeat() {
    LOG_DEBUG("Heartbeat is disabled for this device; enableHeartbeat() ignored");
}

void EgoDeviceMonitor::disableHeartbeat() {}

bool EgoDeviceMonitor::isHeartbeatEnabled() const {
    return false;
}

void EgoDeviceMonitor::pauseHeartbeat() {}

void EgoDeviceMonitor::resumeHeartbeat() {
    LOG_DEBUG("Heartbeat is disabled for this device; resumeHeartbeat() ignored");
}

void EgoDeviceMonitor::enableFirmwareLog() {
    LOG_DEBUG("DeviceMonitor firmware log is disabled for this device; enableFirmwareLog() ignored");
}

void EgoDeviceMonitor::disableFirmwareLog() {}

bool EgoDeviceMonitor::isFirmwareLogEnabled() const {
    return false;
}

}  // namespace libobsensor

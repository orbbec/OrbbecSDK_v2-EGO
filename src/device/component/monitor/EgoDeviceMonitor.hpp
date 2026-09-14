// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceMonitor.hpp"

namespace libobsensor {

/**
 * @brief Ego-family device monitor that only retains synchronous vendor command transport.
 *
 * @details Heartbeat, legacy firmware-log polling and generic device-state monitoring are
 * disabled so the DeviceMonitor background polling thread cannot be started. The inherited
 * sendAndReceiveData() implementation remains available.
 */
class EgoDeviceMonitor : public DeviceMonitor {
public:
    using DeviceMonitor::DeviceMonitor;

    OBDeviceState getCurrentDeviceState() const override;
    int           registerStateChangedCallback(DeviceStateChangedCallback callback) override;
    void          unregisterStateChangedCallback(int callbackId) override;

    void enableHeartbeat() override;
    void disableHeartbeat() override;
    bool isHeartbeatEnabled() const override;
    void pauseHeartbeat() override;
    void resumeHeartbeat() override;

    void enableFirmwareLog() override;
    void disableFirmwareLog() override;
    bool isFirmwareLogEnabled() const override;
};

}  // namespace libobsensor

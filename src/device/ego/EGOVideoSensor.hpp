// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "sensor/video/VideoSensor.hpp"

namespace libobsensor {

// EGO exposes an independent Orbbec vendor XU on each applicable UVC interface. VideoSensor's
// default implementation routes a stop property through the device-global PropertyServer, which
// can bind the property to a different UVC port. This class sends a configured stop property on the
// sensor's own backend port instead.
class EGOVideoSensor : public VideoSensor {
public:
    EGOVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend, uint32_t stopStreamPropertyId);
    ~EGOVideoSensor() noexcept override;

protected:
    void trySendStopStreamVendorCmd() override;

private:
    uint32_t stopStreamPropertyId_;
};

}  // namespace libobsensor

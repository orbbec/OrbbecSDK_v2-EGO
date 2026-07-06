// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "sensor/video/VideoSensor.hpp"

namespace libobsensor {

// EGO exposes an independent Orbbec vendor XU on each color UVC interface (left and right).
// VideoSensor's default trySendStopStreamVendorCmd() routes OB_PROP_STOP_COLOR_STREAM_BOOL through
// the device-global PropertyServer, which keys by property id and can therefore bind the stop-color
// command to only one port. This subclass instead sends it on the sensor's own backend port, so that
// stopping the left/right color stream targets that eye's own interface XU; the firmware tells left
// from right by the UVC interface the control transfer arrives on.
class EGOColorVideoSensor : public VideoSensor {
public:
    EGOColorVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend);
    ~EGOColorVideoSensor() noexcept override;

protected:
    void trySendStopStreamVendorCmd() override;
};

}  // namespace libobsensor

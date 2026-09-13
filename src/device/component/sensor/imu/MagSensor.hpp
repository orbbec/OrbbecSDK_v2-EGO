// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "sensor/SensorBase.hpp"
#include "IStreamer.hpp"

namespace libobsensor {
class MagSensor : public SensorBase {
public:
    MagSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend, const std::shared_ptr<IStreamer> &streamer,
              bool enableStreamStrategyValidation = false);
    ~MagSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:
    std::shared_ptr<IStreamer> streamer_;
    bool                       enableStreamStrategyValidation_;
};
}  // namespace libobsensor

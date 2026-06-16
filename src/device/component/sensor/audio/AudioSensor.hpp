// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "sensor/SensorBase.hpp"
#include "IFrame.hpp"
#include "frame/SpscFrameQueue.hpp"

#include <memory>

namespace libobsensor {

// Audio (microphone) sensor. Backend is an IVideoStreamPort-style port (RTSP for the WiFi path,
// UAC for the USB path) that delivers PCM chunks. Audio is an independent callback stream that
// does NOT participate in FrameSet aggregation in the first version (see TN2608 plan §5.1.3).
class AudioSensor : public SensorBase {
public:
    AudioSensor(IDevice *owner, const std::shared_ptr<ISourcePort> &backend);
    ~AudioSensor() noexcept override;

    void start(std::shared_ptr<const StreamProfile> sp, FrameCallback callback) override;
    void stop() override;

private:
    void onBackendFrameCallback(std::shared_ptr<Frame> frame);

private:
    std::shared_ptr<const StreamProfile>   currentBackendStreamProfile_;
    std::shared_ptr<SpscFrameQueue<Frame>> frameQueue_;
};

}  // namespace libobsensor

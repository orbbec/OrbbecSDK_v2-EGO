// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "libobsensor/h/ObTypes.h"

#include "AudioTimestampFitter.hpp"
#include "ISourcePort.hpp"
#include "stream/StreamProfile.hpp"
#include "audio/IAudioChannelCountConfigurable.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

struct IMMDevice;
struct IAudioClient;
struct IAudioCaptureClient;

namespace libobsensor {

// WASAPI-based USB Audio Class capture port for Windows. The system usbaudio.sys driver already exposes
// the UAC microphone as a standard capture endpoint, so libusb is not used here. The endpoint is paired
// to the EGO device by matching the USB VID/PID carried in PKEY_Device_InstanceId.
class WasapiUacDevicePort : public IVideoStreamPort, public IAudioChannelCountConfigurable {
public:
    explicit WasapiUacDevicePort(std::shared_ptr<const USBSourcePortInfo> portInfo);
    ~WasapiUacDevicePort() noexcept override;

    StreamProfileList getStreamProfileList() override;
    void              startStream(std::shared_ptr<const StreamProfile> profile, MutableFrameCallback callback) override;
    void              stopStream(std::shared_ptr<const StreamProfile> profile) override;
    void              stopAllStream() override;
    void              setAudioChannelCount(uint32_t channelCount) override;

    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;

private:
    bool bindCaptureEndpoint();
    bool startCaptureClient();
    void stopCaptureClient();
    void captureLoop();
    void capturePackets();
    void reportCaptureStatus(bool failed);
    void releaseCaptureClient();

private:
    std::shared_ptr<const USBSourcePortInfo> portInfo_;

    // Output format. Device classes set the channel count; sample rate and bit depth
    // remain the SDK's fixed output format.
    uint32_t sampleRate_    = 48000;
    uint32_t channelCount_  = 2;
    uint32_t bitsPerSample_ = 16;

    std::shared_ptr<const AudioStreamProfile> activatedProfile_;
    MutableFrameCallback                      frameCallback_;

    std::atomic<bool> streaming_{ false };
    std::thread       captureThread_;
    std::mutex        streamMutex_;
    std::mutex        captureStateMutex_;
    std::condition_variable firstFrameCv_;
    std::atomic<bool>       firstFrameReceived_{ false };
    std::atomic<bool>       captureFailed_{ false };

    // Opaque WASAPI handles (defined in the .cpp to keep COM headers out of this header).
    IMMDevice           *mmDevice_      = nullptr;
    IAudioClient        *audioClient_   = nullptr;
    IAudioCaptureClient *captureClient_ = nullptr;

    AudioTimestampFitter fitter_;
    uint64_t             totalFrames_ = 0;
};

}  // namespace libobsensor

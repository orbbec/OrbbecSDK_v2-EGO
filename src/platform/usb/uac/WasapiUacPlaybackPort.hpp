// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IUsbAudioPlaybackPort.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

struct IMMDevice;
struct IAudioClient;
struct IAudioRenderClient;

namespace libobsensor {
class WasapiUacPlaybackPort : public IUsbAudioPlaybackPort {
public:
    explicit WasapiUacPlaybackPort(std::shared_ptr<const USBSourcePortInfo> info);
    ~WasapiUacPlaybackPort() noexcept override;
    std::shared_ptr<const SourcePortInfo> getSourcePortInfo() const override;
    void openSession() override;
    void beginPlayback(uint64_t generation) override;
    size_t writePcm(const uint8_t *data, size_t size, uint64_t generation) override;
    bool drain(uint64_t generation) override;
    void endPlayback(uint64_t generation) noexcept override;
    void closeSession() noexcept override;

private:
    void renderLoop();
    std::shared_ptr<const USBSourcePortInfo> info_;
    IMMDevice *device_ = nullptr;
    IAudioClient *client_ = nullptr;
    IAudioRenderClient *render_ = nullptr;
    std::atomic<bool> sessionOpen_{ false }, playing_{ false };
    bool comInitialized_ = false;
    uint64_t generation_ = 0;
    std::mutex mutex_;
    std::condition_variable queueCv_, drainCv_;
    std::deque<uint8_t> queue_;
    static constexpr size_t kMaxQueueBytes = 6400;  // 200 ms at 16 kHz mono S16LE
    std::thread thread_;
};
}  // namespace libobsensor

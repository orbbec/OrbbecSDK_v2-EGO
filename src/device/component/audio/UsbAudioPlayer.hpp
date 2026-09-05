// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IUsbAudioPlayer.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>

namespace libobsensor {
class IUsbAudioPlaybackPort;

class UsbAudioPlayer : public IUsbAudioPlayer {
public:
    UsbAudioPlayer(IDevice *owner, std::shared_ptr<IUsbAudioPlaybackPort> port);
    ~UsbAudioPlayer() noexcept override;
    IDevice *getOwner() const override;
    void playFile(const std::string &filePath) override;
    void stop() noexcept override;
private:
    void setOutputEnabled(bool enabled);
    void disableOutputNoThrow() noexcept;
    IDevice *owner_;
    std::shared_ptr<IUsbAudioPlaybackPort> port_;
    std::mutex mutex_;
    std::mutex stopMutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopRequested_{false};
    bool playing_=false, enabled_=false;
    uint64_t generation_=0;
};
}  // namespace libobsensor

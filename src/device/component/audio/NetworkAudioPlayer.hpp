// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "INetworkAudioPlayer.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

namespace libobsensor {

class NetworkAudioPlayer : public INetworkAudioPlayer {
public:
    NetworkAudioPlayer(IDevice *owner, std::string deviceAddress);
    ~NetworkAudioPlayer() noexcept override;

    IDevice *getOwner() const override;
    void     playFile(const std::string &filePath) override;
    void     stop() noexcept override;

private:
    void     setAudioOutputEnabled(bool enabled);
    uint16_t startReceiver();
    void     stopReceiverNoThrow() noexcept;

private:
    IDevice                *owner_;
    std::string             deviceAddress_;
    std::mutex              sessionMutex_;
    std::condition_variable sessionCv_;
    std::mutex              controlMutex_;
    std::mutex              sendMutex_;
    std::atomic<bool>       stopRequested_{ false };
    std::atomic<bool>       receiverStarted_{ false };
    bool                    playing_ = false;
};

}  // namespace libobsensor

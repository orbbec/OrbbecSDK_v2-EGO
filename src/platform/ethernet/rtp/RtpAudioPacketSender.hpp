// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ethernet/socket/SocketTypes.hpp"

#include <cstdint>
#include <string>

namespace libobsensor {

class RtpAudioPacketSender {
public:
    // 20 ms of 16 kHz, mono, S16LE PCM.
    static constexpr uint16_t payloadBytes = 640;

    RtpAudioPacketSender(const std::string &deviceAddress, uint16_t port);
    ~RtpAudioPacketSender() noexcept;

    void send(const uint8_t *pcm, uint16_t pcmSize, uint16_t sequence, uint32_t timestamp, uint32_t ssrc);

private:
    SOCKET      socket_ = INVALID_SOCKET;
    sockaddr_in target_{};
};

}  // namespace libobsensor

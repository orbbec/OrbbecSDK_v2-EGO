// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RtpAudioPacketSender.hpp"

#include "exception/ObException.hpp"
#include "utils/Utils.hpp"

#include <array>
#include <cstring>

namespace libobsensor {
namespace {
constexpr uint16_t rtpHeaderBytes       = 12;
constexpr uint8_t  rtpPayloadType       = 97;
constexpr uint8_t  rtpVersion           = 2;
constexpr uint8_t  rtpVersionBitOffset  = 6;
constexpr uint16_t rtpFlagsOffset       = 0;
constexpr uint16_t rtpPayloadTypeOffset = 1;
constexpr uint16_t rtpSequenceOffset    = 2;
constexpr uint16_t rtpTimestampOffset   = 4;
constexpr uint16_t rtpSsrcOffset        = 8;
}  // namespace

RtpAudioPacketSender::RtpAudioPacketSender(const std::string &deviceAddress, uint16_t port) {
#if defined(WIN32) || defined(_WIN32) || defined(WINCE)
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#else
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
#endif
    if(socket_ == INVALID_SOCKET) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "Failed to create RTP UDP socket, error=" << GET_LAST_ERROR());
    }
    target_.sin_family = AF_INET;
    target_.sin_port   = htons(port);
    if(inet_pton(AF_INET, deviceAddress.c_str(), &target_.sin_addr) != 1) {
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        THROW_INVALID_PARAM_EXCEPTION("Invalid network audio device address: " + deviceAddress);
    }
}

RtpAudioPacketSender::~RtpAudioPacketSender() noexcept {
    if(socket_ != INVALID_SOCKET) {
        closesocket(socket_);
    }
}

void RtpAudioPacketSender::send(const uint8_t *pcm, uint16_t pcmSize, uint16_t sequence, uint32_t timestamp, uint32_t ssrc) {
    std::array<uint8_t, rtpHeaderBytes + payloadBytes> packet{};
    if(!pcm || pcmSize > payloadBytes) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid RTP audio payload");
    }
    packet[rtpFlagsOffset]       = static_cast<uint8_t>(rtpVersion << rtpVersionBitOffset);
    packet[rtpPayloadTypeOffset] = rtpPayloadType;
    const auto netSequence       = htons(sequence);
    const auto netTimestamp      = htonl(timestamp);
    const auto netSsrc           = htonl(ssrc);
    std::memcpy(packet.data() + rtpSequenceOffset, &netSequence, sizeof(netSequence));
    std::memcpy(packet.data() + rtpTimestampOffset, &netTimestamp, sizeof(netTimestamp));
    std::memcpy(packet.data() + rtpSsrcOffset, &netSsrc, sizeof(netSsrc));
    std::memcpy(packet.data() + rtpHeaderBytes, pcm, pcmSize);
    const auto packetSize = static_cast<int>(rtpHeaderBytes + pcmSize);
    if(sendto(socket_, reinterpret_cast<const char *>(packet.data()), packetSize, 0, reinterpret_cast<const sockaddr *>(&target_), sizeof(target_))
       != packetSize) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "Failed to send RTP audio packet, error=" << GET_LAST_ERROR());
    }
}

}  // namespace libobsensor

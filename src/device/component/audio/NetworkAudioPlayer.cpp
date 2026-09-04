// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "NetworkAudioPlayer.hpp"

#include "IDevice.hpp"
#include "ethernet/rtp/RtpAudioPacketSender.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>
#include <thread>
#include <utility>

namespace libobsensor {
namespace {
constexpr uint32_t rtpTimestampPerPacket = 1800;
constexpr uint16_t defaultRtpPort        = 57686;
constexpr uint16_t wavPcmFormat          = 1;
constexpr uint16_t wavChannels           = 1;
constexpr uint32_t wavSampleRate         = 48000;
constexpr uint16_t wavBitsPerSample      = 16;
constexpr uint16_t wavBlockAlign         = wavChannels * (wavBitsPerSample / 8);
constexpr uint32_t wavByteRate           = wavSampleRate * wavBlockAlign;

uint16_t readLe16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16)
           | (static_cast<uint32_t>(data[3]) << 24);
}

struct WavDataInfo {
    std::streamoff offset;
    uint32_t       size;
};

WavDataInfo parseWav(std::ifstream &file) {
    file.seekg(0, std::ios::end);
    const auto fileEnd = file.tellg();
    file.seekg(0, std::ios::beg);
    if(!file || fileEnd == std::streampos(-1) || fileEnd < std::streampos(12)) {
        THROW_INVALID_PARAM_EXCEPTION("Audio file must be a RIFF/WAVE file");
    }

    std::array<uint8_t, 12> riff{};
    file.read(reinterpret_cast<char *>(riff.data()), riff.size());
    if(file.gcount() != static_cast<std::streamsize>(riff.size()) || std::memcmp(riff.data(), "RIFF", 4) != 0 || std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
        THROW_INVALID_PARAM_EXCEPTION("Audio file must be a RIFF/WAVE file");
    }

    bool        fmtFound  = false;
    bool        dataFound = false;
    WavDataInfo result{};
    while(file && !dataFound) {
        std::array<uint8_t, 8> chunkHeader{};
        file.read(reinterpret_cast<char *>(chunkHeader.data()), chunkHeader.size());
        if(file.gcount() == 0) {
            break;
        }
        if(file.gcount() != static_cast<std::streamsize>(chunkHeader.size())) {
            THROW_INVALID_PARAM_EXCEPTION("Truncated WAV chunk header");
        }
        const auto chunkSize  = readLe32(chunkHeader.data() + 4);
        const auto dataStart  = file.tellg();
        const auto chunkBytes = static_cast<uint64_t>(chunkSize) + (chunkSize & 1);
        if(dataStart == std::streampos(-1) || dataStart > fileEnd || chunkBytes > static_cast<uint64_t>(fileEnd - dataStart)) {
            THROW_INVALID_PARAM_EXCEPTION("Truncated WAV chunk data");
        }
        if(std::memcmp(chunkHeader.data(), "fmt ", 4) == 0) {
            if(fmtFound || chunkSize < 16) {
                THROW_INVALID_PARAM_EXCEPTION("Invalid WAV fmt chunk");
            }
            std::array<uint8_t, 16> fmt{};
            file.read(reinterpret_cast<char *>(fmt.data()), fmt.size());
            if(file.gcount() != static_cast<std::streamsize>(fmt.size())) {
                THROW_INVALID_PARAM_EXCEPTION("Truncated WAV fmt chunk");
            }
            const auto format        = readLe16(fmt.data());
            const auto channels      = readLe16(fmt.data() + 2);
            const auto sampleRate    = readLe32(fmt.data() + 4);
            const auto byteRate      = readLe32(fmt.data() + 8);
            const auto blockAlign    = readLe16(fmt.data() + 12);
            const auto bitsPerSample = readLe16(fmt.data() + 14);
            if(format != wavPcmFormat || channels != wavChannels || sampleRate != wavSampleRate || byteRate != wavByteRate || blockAlign != wavBlockAlign
               || bitsPerSample != wavBitsPerSample) {
                LOG_WARN("Unsupported WAV format: encoding={}, channels={}, sample rate={} Hz, byte rate={} bytes/s, block align={}, bits per sample={}; "
                         "required: PCM (encoding {}), {} channel, {} Hz, {} bytes/s, block align {}, {} bits per sample",
                         format, channels, sampleRate, byteRate, blockAlign, bitsPerSample, wavPcmFormat, wavChannels, wavSampleRate, wavByteRate,
                         wavBlockAlign, wavBitsPerSample);
                THROW_INVALID_PARAM_EXCEPTION("WAV must be PCM, 48 kHz, mono, 16-bit little-endian");
            }
            fmtFound = true;
        }
        else if(std::memcmp(chunkHeader.data(), "data", 4) == 0) {
            if(dataFound || !fmtFound || chunkSize == 0 || (chunkSize % 2) != 0) {
                THROW_INVALID_PARAM_EXCEPTION("Invalid WAV data chunk");
            }
            result    = { dataStart, chunkSize };
            dataFound = true;
        }
        file.seekg(dataStart + static_cast<std::streamoff>(chunkSize) + static_cast<std::streamoff>(chunkSize & 1));
        if(!file) {
            THROW_INVALID_PARAM_EXCEPTION("Truncated WAV chunk data");
        }
    }
    if(!fmtFound || !dataFound) {
        THROW_INVALID_PARAM_EXCEPTION("WAV requires one fmt chunk and one data chunk");
    }
    return result;
}
}  // namespace

NetworkAudioPlayer::NetworkAudioPlayer(IDevice *owner, std::string deviceAddress) : owner_(owner), deviceAddress_(std::move(deviceAddress)) {}

NetworkAudioPlayer::~NetworkAudioPlayer() noexcept {
    stop();
    std::unique_lock<std::mutex> lock(sessionMutex_);
    sessionCv_.wait(lock, [this] { return !playing_; });
}

IDevice *NetworkAudioPlayer::getOwner() const {
    return owner_;
}

void NetworkAudioPlayer::sendControl(NetAudioAction action, int32_t param) {
    const NetAudioControl control{ static_cast<int32_t>(action), param };
    owner_->getPropertyServer()->setStructureDataT(OB_STRUCT_NET_AUDIO_PLAYER_CONTROL, control);
}

uint16_t NetworkAudioPlayer::startReceiver() {
    std::lock_guard<std::mutex> lock(controlMutex_);
    if(stopRequested_) {
        return 0;
    }
    if(!receiverStarted_) {
        sendControl(NET_AUDIO_ACTION_START, defaultRtpPort);
        receiverStarted_ = true;
        LOG_INFO("Network audio receiver started on port {}", defaultRtpPort);
    }
    return defaultRtpPort;
}

void NetworkAudioPlayer::stopReceiverNoThrow() noexcept {
    std::lock_guard<std::mutex> lock(controlMutex_);
    if(receiverStarted_.exchange(false)) {
        std::lock_guard<std::mutex> sendLock(sendMutex_);
        TRY_EXECUTE({ sendControl(NET_AUDIO_ACTION_STOP, 0); });
        LOG_INFO("Network audio receiver stopped");
    }
}

void NetworkAudioPlayer::playFile(const std::string &filePath) {
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        if(playing_) {
            THROW_RESOURCE_BUSY_EXCEPTION("A network audio playback session is already active");
        }
        playing_       = true;
        stopRequested_ = false;
    }

    try {
        std::ifstream file(filePath, std::ios::binary);
        if(!file.is_open()) {
            THROW_INVALID_PARAM_EXCEPTION("Unable to open WAV file: " + filePath);
        }
        const auto wav     = parseWav(file);
        LOG_INFO("Start playing audio file: {}", filePath);
        const auto rtpPort = startReceiver();
        if(rtpPort == 0 || stopRequested_) {
            {
                std::lock_guard<std::mutex> lock(sessionMutex_);
                playing_ = false;
            }
            sessionCv_.notify_all();
            return;
        }

        RtpAudioPacketSender                                    sender(deviceAddress_, rtpPort);
        std::array<uint8_t, RtpAudioPacketSender::payloadBytes> pcm{};
        std::random_device                                      rd;
        uint16_t                                                sequence  = 0;
        uint32_t                                                timestamp = 0;
        const uint32_t                                          ssrc      = (static_cast<uint32_t>(rd()) << 16) ^ static_cast<uint32_t>(rd());
        auto                                                    deadline  = std::chrono::steady_clock::now();

        file.clear();
        file.seekg(wav.offset);
        uint32_t remaining = wav.size;
        while(remaining > 0 && !stopRequested_) {
            const auto bytesToRead = static_cast<std::streamsize>(std::min<uint32_t>(remaining, RtpAudioPacketSender::payloadBytes));
            pcm.fill(0);
            file.read(reinterpret_cast<char *>(pcm.data()), bytesToRead);
            if(file.gcount() != bytesToRead) {
                THROW_IO_EXCEPTION("Failed to read WAV data");
            }
            {
                std::lock_guard<std::mutex> lock(sendMutex_);
                if(stopRequested_) {
                    break;
                }
                sender.send(pcm.data(), RtpAudioPacketSender::payloadBytes, sequence++, timestamp, ssrc);
            }
            timestamp += rtpTimestampPerPacket;
            remaining -= static_cast<uint32_t>(bytesToRead);
            deadline += std::chrono::milliseconds(20);
            std::this_thread::sleep_until(deadline);
        }
        LOG_INFO("Audio file playback {}: {}", stopRequested_ ? "stopped" : "completed", filePath);
    }
    catch(...) {
        stopReceiverNoThrow();
        {
            std::lock_guard<std::mutex> lock(sessionMutex_);
            playing_ = false;
        }
        sessionCv_.notify_all();
        throw;
    }

    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        playing_ = false;
    }
    sessionCv_.notify_all();
}

void NetworkAudioPlayer::stop() noexcept {
    stopRequested_ = true;
    stopReceiverNoThrow();
}

}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "UsbAudioPlayer.hpp"
#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "property/PropertyServer.hpp"
#include "usb/uac/IUsbAudioPlaybackPort.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <utility>
#include <vector>

namespace libobsensor {
namespace {
uint16_t le16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
uint32_t le32(const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); }

struct WavInfo {
    uint32_t sampleRate = 0;
    uint32_t dataBytes  = 0;
};

WavInfo parseWav(std::ifstream &file) {
    file.seekg(0, std::ios::end);
    const auto fileEnd = file.tellg();
    file.seekg(0, std::ios::beg);
    if(fileEnd < std::streampos(12)) THROW_INVALID_PARAM_EXCEPTION("USB EGO playback requires a complete RIFF/WAVE file");
    std::array<uint8_t, 12> riff{};
    file.read(reinterpret_cast<char *>(riff.data()), riff.size());
    if(file.gcount() != static_cast<std::streamsize>(riff.size()) || std::memcmp(riff.data(), "RIFF", 4) != 0
       || std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
        THROW_INVALID_PARAM_EXCEPTION("USB EGO playback requires a RIFF/WAVE file");
    }

    bool haveFormat = false;
    WavInfo result;
    while(file && !result.dataBytes) {
        std::array<uint8_t, 8> chunk{};
        file.read(reinterpret_cast<char *>(chunk.data()), chunk.size());
        if(file.gcount() != static_cast<std::streamsize>(chunk.size())) break;
        const auto chunkSize = le32(chunk.data() + 4);
        const auto chunkStart = file.tellg();
        const auto paddedSize = static_cast<uint64_t>(chunkSize) + (chunkSize & 1);
        if(chunkStart == std::streampos(-1) || chunkStart > fileEnd || paddedSize > static_cast<uint64_t>(fileEnd - chunkStart)) {
            THROW_INVALID_PARAM_EXCEPTION("Truncated WAV chunk");
        }
        if(std::memcmp(chunk.data(), "fmt ", 4) == 0) {
            if(chunkSize < 16 || chunkSize > 4096) THROW_INVALID_PARAM_EXCEPTION("Invalid WAV format chunk");
            std::vector<uint8_t> format(chunkSize);
            file.read(reinterpret_cast<char *>(format.data()), chunkSize);
            const auto sampleRate = le32(format.data() + 4);
            if(file.gcount() != static_cast<std::streamsize>(chunkSize) || le16(format.data()) != 1 || le16(format.data() + 2) != 1
               || le16(format.data() + 12) != 2 || le16(format.data() + 14) != 16 || sampleRate != 16000
               || le32(format.data() + 8) != sampleRate * 2) {
                THROW_INVALID_PARAM_EXCEPTION("USB EGO playback requires PCM WAV: 16 kHz, mono, 16-bit little-endian");
            }
            result.sampleRate = sampleRate;
            haveFormat        = true;
        }
        else if(std::memcmp(chunk.data(), "data", 4) == 0) {
            if(!haveFormat) THROW_INVALID_PARAM_EXCEPTION("WAV data chunk precedes format chunk");
            result.dataBytes = chunkSize;
            break;  // Keep the stream positioned at the PCM payload.
        }
        else {
            file.seekg(chunkSize, std::ios::cur);
        }
        if(chunkSize & 1) file.seekg(1, std::ios::cur);  // RIFF chunks are word aligned.
    }
    if(!haveFormat || !result.dataBytes) THROW_INVALID_PARAM_EXCEPTION("WAV file has no playable PCM data chunk");
    return result;
}

}
UsbAudioPlayer::UsbAudioPlayer(IDevice *owner,std::shared_ptr<IUsbAudioPlaybackPort> port):owner_(owner),port_(std::move(port)){}
UsbAudioPlayer::~UsbAudioPlayer() noexcept { stop(); }
IDevice *UsbAudioPlayer::getOwner() const{return owner_;}
void UsbAudioPlayer::setOutputEnabled(bool enabled){owner_->getPropertyServer()->setPropertyValueT<bool>(OB_PROP_DEVICE_AUDIO_OUTPUT_ENABLE_BOOL,enabled);}
void UsbAudioPlayer::disableOutputNoThrow() noexcept {
    if(!enabled_) return;
    try {
        setOutputEnabled(false);
    }
    catch(const libobsensor_exception &e) {
        // On Windows, the UVC XU response to the disable command may be
        // aborted while the audio endpoint is being torn down.  The session
        // has already stopped, so treat this as best-effort cleanup.
        LOG_WARN("USB audio output disable (property 304) did not receive an acknowledgement during stop; "
                 "status={}, message={}", static_cast<uint32_t>(e.getStatus()), e.getMessage());
    }
    catch(const std::exception &e) {
        LOG_WARN("USB audio output disable (property 304) failed during stop: {}", e.what());
    }
    catch(...) {
        LOG_WARN("USB audio output disable (property 304) failed during stop");
    }
    // The SET phase may have reached the device even when its XU response was
    // aborted.  Avoid issuing duplicate disable requests; a later play always
    // explicitly enables 304 again before opening the output session.
    enabled_ = false;
}
void UsbAudioPlayer::playFile(const std::string &path) {
    std::unique_lock<std::mutex> l(mutex_); if(playing_) THROW_RESOURCE_BUSY_EXCEPTION("An audio playback session is already active"); playing_=true; stopRequested_=false; const auto gen=++generation_; l.unlock();
    try {
        std::ifstream f(path,std::ios::binary); if(!f) THROW_INVALID_PARAM_EXCEPTION("Unable to open WAV file: "+path);
        const auto wav = parseWav(f);
        if((wav.dataBytes & 1) != 0) {
            THROW_INVALID_PARAM_EXCEPTION("WAV PCM data must contain whole 16 kHz PCM frames");
        }
        if(!enabled_) {
            const auto outputEnableStart = std::chrono::steady_clock::now();
            setOutputEnabled(true);
            const auto outputEnableMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - outputEnableStart).count();
            LOG_DEBUG("UAC playback: property 304 enabled in {} ms; opening UAC playback session immediately", outputEnableMs);
            port_->openSession();
            enabled_ = true;
        }
        port_->beginPlayback(gen);
        uint32_t remaining = wav.dataBytes;
        std::array<uint8_t, 640> pcm{};  // 20 ms at 16 kHz mono S16LE.
        while(!stopRequested_ && remaining > 0) {
            const auto wanted = std::min<uint32_t>(remaining, static_cast<uint32_t>(pcm.size()));
            const auto aligned = wanted & ~uint32_t(1);
            if(aligned == 0) THROW_INVALID_PARAM_EXCEPTION("WAV PCM data is not aligned to the requested sample rate");
            f.read(reinterpret_cast<char *>(pcm.data()), aligned);
            const auto read = static_cast<size_t>(f.gcount()) & ~size_t(1);
            if(read != aligned) THROW_IO_EXCEPTION("Failed to read WAV PCM data");
            remaining -= static_cast<uint32_t>(read);
            if(port_->writePcm(pcm.data(), read, gen) != read) {
                if(!stopRequested_) THROW_IO_EXCEPTION("USB audio playback transport stopped unexpectedly");
                break;
            }
        }
        if(!stopRequested_) {
            const auto drained = port_->drain(gen);
            if(!drained && !stopRequested_) {
                THROW_IO_EXCEPTION("USB audio playback transport stopped before PCM data drained");
            }
        }
        port_->endPlayback(gen);
    } catch(...) { port_->closeSession(); disableOutputNoThrow(); l.lock();playing_=false;l.unlock();cv_.notify_all();throw; }
    l.lock();playing_=false;l.unlock();cv_.notify_all();
}
void UsbAudioPlayer::stop() noexcept {
    std::lock_guard<std::mutex> stopLock(stopMutex_);
    stopRequested_ = true;
    uint64_t generation;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        generation = generation_;
    }
    // Wake a producer blocked on transport back-pressure before waiting for it.
    port_->endPlayback(generation);
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !playing_; });
    }
    port_->closeSession();
    disableOutputNoThrow();
}
}  // namespace libobsensor

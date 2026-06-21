// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// EGO sample: render the dual RGB (left + right) streams and play back the device microphone audio.
// Audio playback is cross-platform:
//   - Windows: WinMM waveOut
//   - Linux / arm64: ALSA (when libasound is available; define OB_EGO_HAVE_ALSA)
//   - other platforms: no-op (video still works)

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

// ============================ Cross-platform audio playback ============================
#if defined(_WIN32)

#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>  // WAVE_FORMAT_IEEE_FLOAT

class AudioPlayer {
public:
    ~AudioPlayer() {
        close();
    }

    bool open(uint32_t sampleRate, uint32_t channels, uint32_t bitsPerSample) {
        if(sampleRate == 0 || channels == 0 || bitsPerSample == 0) {
            return false;
        }
        WAVEFORMATEX wfx = {};
        // WASAPI shared-mode capture usually delivers 32-bit IEEE float; 16/24-bit are integer PCM.
        wfx.wFormatTag      = (bitsPerSample == 32) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM;
        wfx.nChannels       = static_cast<WORD>(channels);
        wfx.nSamplesPerSec  = sampleRate;
        wfx.wBitsPerSample  = static_cast<WORD>(bitsPerSample);
        wfx.nBlockAlign     = static_cast<WORD>(channels * bitsPerSample / 8);
        wfx.nAvgBytesPerSec = sampleRate * wfx.nBlockAlign;
        wfx.cbSize          = 0;

        if(waveOutOpen(&hwo_, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
            return false;
        }
        opened_ = true;
        return true;
    }

    void play(const uint8_t *data, size_t size) {
        if(!opened_ || data == nullptr || size == 0) {
            return;
        }
        std::lock_guard<std::mutex> lk(mutex_);
        recycleLocked();

        auto *hdr           = new WAVEHDR();
        *hdr                = WAVEHDR();
        hdr->lpData         = reinterpret_cast<LPSTR>(malloc(size));
        hdr->dwBufferLength = static_cast<DWORD>(size);
        memcpy(hdr->lpData, data, size);

        if(waveOutPrepareHeader(hwo_, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR || waveOutWrite(hwo_, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) {
            free(hdr->lpData);
            delete hdr;
            return;
        }
        pending_.push_back(hdr);
    }

    void close() {
        std::lock_guard<std::mutex> lk(mutex_);
        if(!opened_) {
            return;
        }
        waveOutReset(hwo_);
        for(auto *hdr: pending_) {
            waveOutUnprepareHeader(hwo_, hdr, sizeof(WAVEHDR));
            free(hdr->lpData);
            delete hdr;
        }
        pending_.clear();
        waveOutClose(hwo_);
        opened_ = false;
    }

private:
    // Free buffers the device has finished playing. Caller must hold mutex_.
    void recycleLocked() {
        for(auto it = pending_.begin(); it != pending_.end();) {
            if((*it)->dwFlags & WHDR_DONE) {
                waveOutUnprepareHeader(hwo_, *it, sizeof(WAVEHDR));
                free((*it)->lpData);
                delete *it;
                it = pending_.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    HWAVEOUT               hwo_    = nullptr;
    bool                   opened_ = false;
    std::mutex             mutex_;
    std::vector<WAVEHDR *> pending_;
};

#elif defined(OB_EGO_HAVE_ALSA)

#include <alsa/asoundlib.h>

class AudioPlayer {
public:
    ~AudioPlayer() {
        close();
    }

    bool open(uint32_t sampleRate, uint32_t channels, uint32_t bitsPerSample) {
        if(sampleRate == 0 || channels == 0 || bitsPerSample == 0) {
            return false;
        }
        if(snd_pcm_open(&pcm_, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
            pcm_ = nullptr;
            return false;
        }
        // 32-bit samples are treated as IEEE float (WASAPI/most capture mix formats); otherwise signed 16-bit PCM.
        snd_pcm_format_t format = (bitsPerSample == 32) ? SND_PCM_FORMAT_FLOAT_LE : SND_PCM_FORMAT_S16_LE;
        if(snd_pcm_set_params(pcm_, format, SND_PCM_ACCESS_RW_INTERLEAVED, channels, sampleRate, 1 /*soft resample*/, 100000 /*100ms latency*/) < 0) {
            snd_pcm_close(pcm_);
            pcm_ = nullptr;
            return false;
        }
        bytesPerFrame_ = channels * (bitsPerSample / 8);
        opened_        = (bytesPerFrame_ > 0);
        return opened_;
    }

    void play(const uint8_t *data, size_t size) {
        if(!opened_ || data == nullptr || size == 0 || bytesPerFrame_ == 0) {
            return;
        }
        snd_pcm_uframes_t frames = size / bytesPerFrame_;
        if(frames == 0) {
            return;
        }
        std::lock_guard<std::mutex> lk(mutex_);
        snd_pcm_sframes_t           written = snd_pcm_writei(pcm_, data, frames);
        if(written < 0) {
            // Recover from underrun / suspend and continue.
            snd_pcm_recover(pcm_, static_cast<int>(written), 1);
        }
    }

    void close() {
        std::lock_guard<std::mutex> lk(mutex_);
        if(!opened_) {
            return;
        }
        snd_pcm_drain(pcm_);
        snd_pcm_close(pcm_);
        pcm_    = nullptr;
        opened_ = false;
    }

private:
    snd_pcm_t *pcm_           = nullptr;
    bool       opened_        = false;
    uint32_t   bytesPerFrame_ = 0;
    std::mutex mutex_;
};

#else  // no audio backend available on this platform

class AudioPlayer {
public:
    bool open(uint32_t, uint32_t, uint32_t) {
        return false;
    }
    void play(const uint8_t *, size_t) {}
    void close() {}
};

#endif
// ======================================================================================

int main(void) try {

    // Create a pipeline with default device.
    ob::Pipeline pipe;

    // Configure which streams to enable. The EGO device exposes two RGB cameras (left + right) plus
    // audio (microphone).
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_COLOR_LEFT);
    config->enableVideoStream(OB_STREAM_COLOR_RIGHT);

    // Start the pipeline with config.
    pipe.start(config);

    // ---- Audio: start the audio sensor directly and play it back ----
    std::shared_ptr<ob::Sensor> audioSensor;
    AudioPlayer                 audioPlayer;
    try {
        auto device = pipe.getDevice();
        audioSensor = device->getSensor(OB_SENSOR_AUDIO);
        if(audioSensor) {
            auto profiles = audioSensor->getStreamProfileList();
            if(profiles && profiles->getCount() > 0) {
                auto     profile       = profiles->getProfile(0);
                uint32_t sampleRate    = 0;
                uint32_t channelCount  = 0;
                uint32_t bitsPerSample = 0;
                if(profile->is<ob::AudioStreamProfile>()) {
                    auto audioProfile = profile->as<ob::AudioStreamProfile>();
                    sampleRate        = audioProfile->getSampleRate();
                    channelCount      = audioProfile->getChannelCount();
                    bitsPerSample     = audioProfile->getBitsPerSample();
                    std::cout << "Audio: " << sampleRate << " Hz, " << channelCount << " ch, " << bitsPerSample << " bit" << std::endl;
                }

                bool audioReady = audioPlayer.open(sampleRate, channelCount, bitsPerSample);
                audioSensor->start(profile, [&audioPlayer, audioReady](std::shared_ptr<ob::Frame> frame) {
                    if(!frame) {
                        return;
                    }
                    if(audioReady) {
                        audioPlayer.play(frame->getData(), frame->getDataSize());
                    }

                    // Audio-stream health log (all platforms, independent of playback backend): confirm the first
                    // frame as soon as it arrives, then report the frame rate every 3 seconds. If nothing is printed
                    // here, no audio frames are being delivered.
                    static auto     lastLog    = std::chrono::steady_clock::now();
                    static uint64_t frameCount = 0;
                    static bool     firstFrame = true;
                    if(firstFrame) {
                        firstFrame = false;
                        lastLog    = std::chrono::steady_clock::now();
                        std::cout << "Audio streaming started: first frame " << frame->getDataSize() << " bytes" << std::endl;
                    }
                    ++frameCount;
                    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastLog).count();
                    if(elapsedMs >= 3000) {
                        std::cout << "Audio frame rate: " << (frameCount * 1000.0 / elapsedMs) << " fps" << std::endl;
                        frameCount = 0;
                        lastLog    = std::chrono::steady_clock::now();
                    }
                });
            }
        }
    }
    catch(ob::Error &e) {
        // Audio is optional; keep streaming video even if the microphone is unavailable.
        std::cerr << "Audio unavailable: " << e.what() << std::endl;
        audioSensor = nullptr;
    }

    // Create a window for rendering and set the resolution of the window.
    ob_smpl::CVWindow win("EGO: Dual Color (+ Audio)");

    while(win.run()) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset();
        if(frameSet == nullptr) {
            continue;
        }

        // Render every frame in the set (left + right color).
        win.pushFramesToView(frameSet);
    }

    // Stop audio before stopping the pipeline.
    if(audioSensor) {
        try {
            audioSensor->stop();
        }
        catch(ob::Error &) {
        }
    }
    audioPlayer.close();

    // Stop the Pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

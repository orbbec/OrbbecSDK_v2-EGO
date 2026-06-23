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
#include <fstream>
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

// ============================ WAV file writer (cross-platform) ========================
// Writes captured audio to a .wav file. The RIFF/data sizes are unknown while recording,
// so a placeholder header is written first and patched with the real sizes on close.
class WavWriter {
public:
    ~WavWriter() {
        close();
    }

    bool open(const std::string &path, uint32_t sampleRate, uint32_t channels, uint32_t bitsPerSample) {
        if(sampleRate == 0 || channels == 0 || bitsPerSample == 0) {
            return false;
        }
        file_.open(path, std::ios::binary | std::ios::trunc);
        if(!file_.is_open()) {
            return false;
        }
        sampleRate_    = sampleRate;
        channels_      = channels;
        bitsPerSample_ = bitsPerSample;
        dataBytes_     = 0;
        // 32-bit samples are IEEE float (matches the playback path); otherwise integer PCM.
        formatTag_ = (bitsPerSample == 32) ? 3 /*WAVE_FORMAT_IEEE_FLOAT*/ : 1 /*WAVE_FORMAT_PCM*/;
        writeHeader();  // placeholder; patched on close()
        opened_ = true;
        return true;
    }

    void write(const uint8_t *data, size_t size) {
        if(!opened_ || data == nullptr || size == 0) {
            return;
        }
        std::lock_guard<std::mutex> lk(mutex_);
        file_.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
        dataBytes_ += static_cast<uint32_t>(size);
    }

    void close() {
        std::lock_guard<std::mutex> lk(mutex_);
        if(!opened_) {
            return;
        }
        file_.seekp(0, std::ios::beg);
        writeHeader();  // rewrite with the final sizes
        file_.close();
        opened_ = false;
    }

private:
    void writeLE32(uint32_t v) {
        uint8_t b[4] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24) };
        file_.write(reinterpret_cast<const char *>(b), 4);
    }

    void writeLE16(uint16_t v) {
        uint8_t b[2] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8) };
        file_.write(reinterpret_cast<const char *>(b), 2);
    }

    void writeHeader() {
        uint16_t blockAlign = static_cast<uint16_t>(channels_ * bitsPerSample_ / 8);
        uint32_t byteRate   = sampleRate_ * blockAlign;

        file_.write("RIFF", 4);
        writeLE32(36 + dataBytes_);  // RIFF chunk size
        file_.write("WAVE", 4);

        file_.write("fmt ", 4);
        writeLE32(16);  // fmt chunk size (PCM)
        writeLE16(static_cast<uint16_t>(formatTag_));
        writeLE16(static_cast<uint16_t>(channels_));
        writeLE32(sampleRate_);
        writeLE32(byteRate);
        writeLE16(blockAlign);
        writeLE16(static_cast<uint16_t>(bitsPerSample_));

        file_.write("data", 4);
        writeLE32(dataBytes_);  // data chunk size
    }

    std::ofstream file_;
    bool          opened_        = false;
    uint32_t      sampleRate_    = 0;
    uint32_t      channels_      = 0;
    uint32_t      bitsPerSample_ = 0;
    uint32_t      formatTag_     = 1;
    uint32_t      dataBytes_     = 0;
    std::mutex    mutex_;
};
// ======================================================================================

static void printImuValue(OBFloat3D val, uint64_t index, uint64_t tsUs, float temp, OBFrameType type, const char *unit) {
    auto typeStr = ob::TypeHelper::convertOBFrameTypeToString(type);
    std::cout << typeStr << " #" << index << " tsp=" << tsUs << "us temp=" << temp << "C"
              << " x=" << val.x << unit << " y=" << val.y << unit << " z=" << val.z << unit << std::endl;
}

int main(void) try {

    // On Linux, select the UVC backend before opening any device.
    // OB_UVC_BACKEND_TYPE_LIBUVC  - libusb-based libuvc (default on Linux for EGO)
#if defined(__linux__) || defined(__ANDROID__)
    ob::Context ctx;
    ctx.setUvcBackendType(OB_UVC_BACKEND_TYPE_LIBUVC);
#endif

    // Create a pipeline with default device.
    ob::Pipeline pipe;

    // Configure which streams to enable. The EGO device exposes two RGB cameras (left + right) plus
    // audio (microphone). Both color streams are opened in MJPG format.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_COLOR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_MJPG);
    config->enableVideoStream(OB_STREAM_COLOR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_MJPG);

    // Start the pipeline with config.
    pipe.start(config);

    // ---- Audio: start the audio sensor directly, play it back and save it to a .wav file ----
    std::shared_ptr<ob::Sensor> audioSensor;
    AudioPlayer                 audioPlayer;
    // WavWriter                   wavWriter;
    // const std::string           wavPath = "ego_audio.wav";
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
                // bool wavReady   = wavWriter.open(wavPath, sampleRate, channelCount, bitsPerSample);
                // if(wavReady) {
                //     std::cout << "Audio recording to: " << wavPath << std::endl;
                // }
                // else {
                //     std::cerr << "Failed to open WAV file for recording: " << wavPath << std::endl;
                // }
                audioSensor->start(profile, [&audioPlayer, audioReady](std::shared_ptr<ob::Frame> frame) {
                    if(!frame) {
                        return;
                    }
                    if(audioReady) {
                        audioPlayer.play(frame->getData(), frame->getDataSize());
                    }
                    // if(wavReady) {
                    //     wavWriter.write(frame->getData(), frame->getDataSize());
                    // }

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

    // ---- IMU: dedicated pipeline (separate from video pipeline) with frameset callback ----
    std::shared_ptr<ob::Pipeline> imuPipe;
    try {
        auto device    = pipe.getDevice();
        imuPipe        = std::make_shared<ob::Pipeline>(device);
        auto imuConfig = std::make_shared<ob::Config>();
        imuConfig->enableAccelStream();
        imuConfig->enableGyroStream();
        imuPipe->start(imuConfig, [](std::shared_ptr<ob::FrameSet> frameSet) {
            if(!frameSet) {
                return;
            }
            auto accelFrameRaw = frameSet->getFrame(OB_FRAME_ACCEL);
            if(accelFrameRaw) {
                auto            accelFrame = accelFrameRaw->as<ob::AccelFrame>();
                static uint64_t accelCount = 0;
                if(accelCount % 50 == 0) {
                    printImuValue(accelFrame->getValue(), accelFrame->getIndex(), accelFrame->getTimeStampUs(), accelFrame->getTemperature(),
                                  accelFrame->getType(), "m/s^2");
                }
                ++accelCount;
            }
            auto gyroFrameRaw = frameSet->getFrame(OB_FRAME_GYRO);
            if(gyroFrameRaw) {
                auto            gyroFrame = gyroFrameRaw->as<ob::GyroFrame>();
                static uint64_t gyroCount = 0;
                if(gyroCount % 50 == 0) {
                    printImuValue(gyroFrame->getValue(), gyroFrame->getIndex(), gyroFrame->getTimeStampUs(), gyroFrame->getTemperature(), gyroFrame->getType(),
                                  "rad/s");
                }
                ++gyroCount;
            }
        });
        std::cout << "IMU pipeline started." << std::endl;
    }
    catch(ob::Error &e) {
        // IMU is optional; keep streaming video even if IMU is unavailable.
        std::cerr << "IMU unavailable: " << e.what() << std::endl;
        imuPipe = nullptr;
    }

    // Create a window for rendering and set the resolution of the window. Use ARRANGE_ONE_ROW so that
    // both color streams (left + right) are rendered side by side instead of only the first one.
    ob_smpl::CVWindow win("EGO: Dual Color (+ Audio)", 1280, 720, ob_smpl::ARRANGE_ONE_ROW);

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

    // Stop IMU pipeline before stopping the video pipeline.
    if(imuPipe) {
        try {
            imuPipe->stop();
        }
        catch(ob::Error &) {
        }
    }
    // wavWriter.close();  // finalize the .wav header with the recorded sizes

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

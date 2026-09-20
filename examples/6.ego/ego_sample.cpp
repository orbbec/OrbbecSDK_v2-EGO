// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// EGO/Wrist sample: render the available RGB stream(s) and play back the device microphone audio when present.
// Audio playback is cross-platform:
//   - Windows: WinMM waveOut
//   - Linux / arm64: ALSA (when libasound is available; define OB_EGO_HAVE_ALSA)
//   - other platforms: no-op (video still works)

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"
#include "utils_opencv.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
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

static const char *egoWorkStateToString(OBEgoWorkState state) {
    switch(state) {
    case OB_EGO_WORK_STATE_BOOTING:
        return "BOOTING";
    case OB_EGO_WORK_STATE_SELF_TESTING:
        return "SELF_TESTING";
    case OB_EGO_WORK_STATE_READY:
        return "READY";
    case OB_EGO_WORK_STATE_RECORD_COUNTDOWN:
        return "RECORD_COUNTDOWN";
    case OB_EGO_WORK_STATE_RECORDING:
        return "RECORDING";
    case OB_EGO_WORK_STATE_STOPPING_RECORD:
        return "STOPPING_RECORD";
    case OB_EGO_WORK_STATE_SAVING:
        return "SAVING";
    case OB_EGO_WORK_STATE_FW_UPGRADING:
        return "FW_UPGRADING";
    case OB_EGO_WORK_STATE_SHUTTING_DOWN:
        return "SHUTTING_DOWN";
    case OB_EGO_WORK_STATE_FAULT:
        return "FAULT";
    default:
        return "UNKNOWN";
    }
}

static void printEgoState(const OBEgoStateReport &state) {
    std::cout << "Ego state: protocol=" << state.protocol_version << " seq=" << state.sequence << " workState=" << egoWorkStateToString(state.work_state)
              << " stateFlags=0x" << std::hex << state.state_flags << " errorFlags=0x" << state.error_flags << std::dec
              << " storageFreeBytes=" << state.storage_free_bytes << std::endl;
}

static bool hasSensorType(const std::shared_ptr<ob::Device> &device, OBSensorType sensorType) {
    auto sensorList = device->getSensorList();
    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        if(sensorList->getSensorType(index) == sensorType) {
            return true;
        }
    }
    return false;
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
    auto         device = ob::DataAcquisitionDevice::fromDevice(pipe.getDevice());

    bool hasColor      = hasSensorType(device, OB_SENSOR_COLOR);
    bool hasLeftColor  = hasSensorType(device, OB_SENSOR_COLOR_LEFT);
    bool hasRightColor = hasSensorType(device, OB_SENSOR_COLOR_RIGHT);
    bool hasLeftIr     = hasSensorType(device, OB_SENSOR_IR_LEFT);
    bool hasRightIr    = hasSensorType(device, OB_SENSOR_IR_RIGHT);

    // Enable each stream independently by its own sensor presence, so the sample adapts to any device layout:
    // Wrist exposes one Color; EGO exposes left/right Color; UMI Finger exposes one Color plus left/right IR.
    // All EGO-family cameras are MJPEG/H264/H265 capable; H264 is used here.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    if(hasColor) {
        config->enableVideoStream(OB_STREAM_COLOR, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
        std::cout << "Enabled COLOR stream." << std::endl;
    }
    if(hasLeftColor) {
        config->enableVideoStream(OB_STREAM_COLOR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
        std::cout << "Enabled COLOR_LEFT stream." << std::endl;
    }
    if(hasRightColor) {
        config->enableVideoStream(OB_STREAM_COLOR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
        std::cout << "Enabled COLOR_RIGHT stream." << std::endl;
    }
    if(hasLeftIr) {
        config->enableVideoStream(OB_STREAM_IR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
        std::cout << "Enabled IR_LEFT stream." << std::endl;
    }
    if(hasRightIr) {
        config->enableVideoStream(OB_STREAM_IR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_H264);
        std::cout << "Enabled IR_RIGHT stream." << std::endl;
    }
    if(!hasColor && !hasLeftColor && !hasRightColor && !hasLeftIr && !hasRightIr) {
        std::cerr << "No color or IR sensor found." << std::endl;
        return EXIT_FAILURE;
    }

    std::string windowTitle = "EGO Sample: Color + IR (+ optional Audio)";

    // Start the pipeline with config.
    pipe.start(config);

    try {
        device->setEgoStateCallback([](const OBEgoStateReport &state) { printEgoState(state); });
        std::cout << "Ego state callback registered." << std::endl;
    }
    catch(ob::Error &e) {
        std::cerr << "Ego state unavailable: " << e.what() << std::endl;
    }

    // ---- Audio: start the audio sensor directly, play it back and save it to a .wav file ----
    std::shared_ptr<ob::Sensor> audioSensor;
    AudioPlayer                 audioPlayer;
    // WavWriter                   wavWriter;
    // const std::string           wavPath = "ego_audio.wav";
    try {
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

    // ---- Magnetic angle encoder (MT6826S, UMI Finger only): start the sensor directly and log the angle ----
    std::shared_ptr<ob::Sensor> magEncoderSensor;
    try {
        if(hasSensorType(device, OB_SENSOR_MAG_ENCODER)) {
            magEncoderSensor = device->getSensor(OB_SENSOR_MAG_ENCODER);
            if(magEncoderSensor) {
                auto profiles = magEncoderSensor->getStreamProfileList();
                if(profiles && profiles->getCount() > 0) {
                    // The encoder carries no range / rate presets, so it has a single stream profile.
                    auto profile = profiles->getProfile(0);
                    magEncoderSensor->start(profile, [](std::shared_ptr<ob::Frame> frame) {
                        if(!frame || frame->getDataSize() < sizeof(OBMagEncoderFrameData)) {
                            return;
                        }
                        static uint64_t magCount = 0;
                        if(magCount++ % 50 != 0) {
                            return;
                        }
                        auto data = reinterpret_cast<const OBMagEncoderFrameData *>(frame->getData());
                        std::cout << "MagEncoder #" << frame->getIndex() << " tsp=" << data->timestampUs << "us " 
                                  << " angleDegree=" << data->angleDegree << "deg status=0x" << std::hex << static_cast<int>(data->status) << std::dec
                                  << std::endl;
                    });
                    std::cout << "Magnetic angle encoder streaming started." << std::endl;
                }
            }
        }
    }
    catch(ob::Error &e) {
        // The encoder is optional; keep streaming video even if it is unavailable.
        std::cerr << "Magnetic angle encoder unavailable: " << e.what() << std::endl;
        magEncoderSensor = nullptr;
    }

    // Create a window for rendering. ARRANGE_ONE_ROW keeps EGO dual color side by side and is harmless for Wrist single color.
    ob_smpl::CVWindow win(windowTitle.c_str(), 1280, 720, ob_smpl::ARRANGE_ONE_ROW);

    // Per-stream frame-rate log: count frames by type and report fps every 3 seconds. This confirms each video
    // stream is actually delivering frames even when the format (e.g. H264) is not rendered in the window.
    std::map<OBFrameType, uint64_t> videoFrameCounts;
    auto                            lastVideoLog = std::chrono::steady_clock::now();

    while(win.run()) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset();
        if(frameSet == nullptr) {
            continue;
        }

        for(uint32_t i = 0; i < frameSet->getCount(); i++) {
            auto frame = frameSet->getFrameByIndex(i);
            if(frame) {
                ++videoFrameCounts[frame->getType()];
            }
        }
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastVideoLog).count();
        if(elapsedMs >= 3000) {
            for(const auto &entry: videoFrameCounts) {
                std::cout << ob::TypeHelper::convertOBFrameTypeToString(entry.first) << " frame rate: " << (entry.second * 1000.0 / elapsedMs) << " fps"
                          << std::endl;
            }
            videoFrameCounts.clear();
            lastVideoLog = std::chrono::steady_clock::now();
        }

        // Render every frame in the set.
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

    // Stop the magnetic angle encoder sensor.
    if(magEncoderSensor) {
        try {
            magEncoderSensor->stop();
        }
        catch(ob::Error &) {
        }
    }

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

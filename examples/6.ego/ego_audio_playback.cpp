// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

constexpr uint16_t defaultDevicePort = 8090;

std::string readInput(const char *prompt) {
    std::cout << prompt;
    std::string input;
    if(!std::getline(std::cin, input)) {
        throw std::runtime_error("Unable to read console input");
    }
    if(input.size() >= 2 && input.front() == '"' && input.back() == '"') {
        input = input.substr(1, input.size() - 2);
    }
    return input;
}

void printVolumeRange(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    if(!device->isPropertySupported(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT, OB_PERMISSION_READ)) {
        std::cout << "Audio volume property (305) is not supported by this device or firmware." << std::endl;
        return;
    }
    const auto range = device->getIntPropertyRange(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT);
    std::cout << "Audio volume (dB): current=" << range.cur << ", min=" << range.min << ", max=" << range.max << ", step=" << range.step
              << ", default=" << range.def << std::endl;
}

void setVolume(const std::shared_ptr<ob::DataAcquisitionDevice> &device, int32_t value) {
    if(!device->isPropertySupported(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT, OB_PERMISSION_WRITE)) {
        std::cout << "Audio volume control is not supported by this device or firmware." << std::endl;
        return;
    }
    const auto range = device->getIntPropertyRange(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT);
    value             = std::max(range.min, std::min(range.max, value));
    device->setIntProperty(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT, value);
    std::cout << "Volume set to " << value << " (range: " << range.min << " ~ " << range.max << ")" << std::endl;
}

class EgoDataStreams {
public:
    explicit EgoDataStreams(std::shared_ptr<ob::Device> device) : device_(std::move(device)) {}
    ~EgoDataStreams() noexcept { stop(); }

    bool isRunning() const { return running_; }

    void start() {
        if(running_) {
            return;
        }

        // Resolve every profile before starting any stream, so a missing exact
        // H264 profile never leaves a partially-started data set behind.
        leftColor_  = device_->getSensor(OB_SENSOR_COLOR_LEFT);
        rightColor_ = device_->getSensor(OB_SENSOR_COLOR_RIGHT);
        audio_      = device_->getSensor(OB_SENSOR_AUDIO);
        accel_      = device_->getSensor(OB_SENSOR_ACCEL);
        gyro_       = device_->getSensor(OB_SENSOR_GYRO);
        if(!leftColor_ || !rightColor_ || !audio_ || !accel_ || !gyro_) {
            throw std::runtime_error("EGO must expose left/right color, audio, accel and gyro sensors");
        }

        const auto leftProfile  = leftColor_->getStreamProfileList()->getVideoStreamProfile(1600, 1200, OB_FORMAT_H264, 30);
        const auto rightProfile = rightColor_->getStreamProfileList()->getVideoStreamProfile(1600, 1200, OB_FORMAT_H264, 30);
        const auto audioProfile = audio_->getStreamProfileList()->getProfile(0);
        const auto accelProfile = accel_->getStreamProfileList()->getProfile(0);
        const auto gyroProfile  = gyro_->getStreamProfileList()->getProfile(0);

        leftFrames_ = rightFrames_ = audioFrames_ = accelFrames_ = gyroFrames_ = 0;
        try {
            leftColor_->start(leftProfile, [this](std::shared_ptr<ob::Frame>) { ++leftFrames_; });
            leftStarted_ = true;
            rightColor_->start(rightProfile, [this](std::shared_ptr<ob::Frame>) { ++rightFrames_; });
            rightStarted_ = true;
            audio_->start(audioProfile, [this](std::shared_ptr<ob::Frame> frame) {
                if(frame && frame->getDataSize() > 0) {
                    ++audioFrames_;
                }
            });
            audioStarted_ = true;
            accel_->start(accelProfile, [this](std::shared_ptr<ob::Frame>) { ++accelFrames_; });
            accelStarted_ = true;
            gyro_->start(gyroProfile, [this](std::shared_ptr<ob::Frame>) { ++gyroFrames_; });
            gyroStarted_ = true;
            running_ = true;
        }
        catch(...) {
            stop();
            throw;
        }
    }

    void stop() noexcept {
        stopSensor(gyro_, gyroStarted_);
        stopSensor(accel_, accelStarted_);
        stopSensor(audio_, audioStarted_);
        stopSensor(rightColor_, rightStarted_);
        stopSensor(leftColor_, leftStarted_);
        running_ = false;
    }

    void printCounts() const {
        std::cout << "Data frames: left=" << leftFrames_.load() << ", right=" << rightFrames_.load() << ", audio=" << audioFrames_.load()
                  << ", accel=" << accelFrames_.load() << ", gyro=" << gyroFrames_.load() << std::endl;
    }

private:
    static void stopSensor(const std::shared_ptr<ob::Sensor> &sensor, bool &started) noexcept {
        if(!started) {
            return;
        }
        try {
            sensor->stop();
        }
        catch(const ob::Error &e) {
            std::cerr << "Data stream stop error: " << e.what() << std::endl;
        }
        catch(const std::exception &e) {
            std::cerr << "Data stream stop error: " << e.what() << std::endl;
        }
        catch(...) {
            std::cerr << "Data stream stop error: unknown exception" << std::endl;
        }
        started = false;
    }

    std::shared_ptr<ob::Device> device_;
    std::shared_ptr<ob::Sensor> leftColor_, rightColor_, audio_, accel_, gyro_;
    bool leftStarted_ = false, rightStarted_ = false, audioStarted_ = false, accelStarted_ = false, gyroStarted_ = false, running_ = false;
    std::atomic<uint64_t> leftFrames_{ 0 }, rightFrames_{ 0 }, audioFrames_{ 0 }, accelFrames_{ 0 }, gyroFrames_{ 0 };
};

}  // namespace

int main() {
    std::thread       playThread;
    std::atomic<bool> isPlaying{ false };

    try {
        std::cout << "EGO audio playback validation\n"
                  << "Network and USB: PCM WAV 16 kHz mono 16-bit.\n\n";
        const auto mode = readInput("Connection [u]sb / [n]etwork: ");
        ob::Context context;
        std::shared_ptr<ob::Device> rawDevice;
        if(mode == "u" || mode == "U") {
            auto list = context.queryDeviceList();
            if(list->getCount() == 0) throw std::runtime_error("No USB EGO device found");
            rawDevice = list->getDevice(0);
            std::cout << "Using first connected USB device." << std::endl;
        }
        else {
            const auto ip = readInput("Enter device IP address: ");
            std::cout << "Connecting to network device at " << ip << ':' << defaultDevicePort << " ..." << std::endl;
            rawDevice = context.createNetDevice(ip.c_str(), defaultDevicePort);
        }
        auto device = ob::DataAcquisitionDevice::fromDevice(rawDevice);
        // fromDevice transfers the underlying Device handle, so all sensor
        // access must use the resulting DataAcquisitionDevice instance.
        EgoDataStreams dataStreams(device);

        printVolumeRange(device);

        std::cout << "\nCommands:\n"
                  << "  <file path>   Play a WAV file\n"
                  << "  s             Stop playback\n"
                  << "  v <value>     Set volume (e.g. v 80)\n"
                  << "  v+            Increase volume by one step\n"
                  << "  v-            Decrease volume by one step\n"
                  << "  d             Toggle left/right H264, audio and IMU data streams (no rendering)\n"
                  << "  q             Stop and quit\n\n";

        auto stopPlayback = [&]() {
            isPlaying = false;
            device->stopAudioPlayback();
            if(playThread.joinable()) {
                playThread.join();
            }
        };

        auto startPlayback = [&](const std::string &filePath) {
            // A naturally completed file deliberately leaves the USB output
            // session enabled.  Do not turn it off before the next file: this
            // is the intended fast-path for consecutive playAudioFile calls.
            // Only interrupt an actually active playback.
            if(isPlaying) {
                stopPlayback();
            }
            else if(playThread.joinable()) {
                playThread.join();
            }
            isPlaying = true;
            playThread = std::thread([&device, &isPlaying, filePath]() {
                try {
                    device->playAudioFile(filePath);
                }
                catch(const ob::Error &e) {
                    std::cerr << "\nPlayback error: " << e.what() << std::endl;
                }
                isPlaying = false;
                std::cout << "\nDone." << std::endl;
            });
        };

        while(true) {
            const auto input = readInput(isPlaying ? "[playing] > " : "> ");

            if(input == "q" || input == "Q") {
                stopPlayback();
                dataStreams.stop();
                break;
            }

            if(input == "d" || input == "D") {
                try {
                    if(dataStreams.isRunning()) {
                        dataStreams.stop();
                        dataStreams.printCounts();
                        std::cout << "Data streams stopped." << std::endl;
                    }
                    else {
                        dataStreams.start();
                        std::cout << "Data streams started: left/right 1600x1200 H264 @ 30 fps, audio, accel and gyro (no rendering)." << std::endl;
                    }
                }
                catch(const ob::Error &e) {
                    dataStreams.stop();
                    std::cerr << "Data stream error: " << e.what() << std::endl;
                }
                catch(const std::exception &e) {
                    dataStreams.stop();
                    std::cerr << "Data stream error: " << e.what() << std::endl;
                }
                continue;
            }

            if(input == "s" || input == "S") {
                stopPlayback();
                std::cout << "Stopped." << std::endl;
                continue;
            }

            if(input == "v+" || input == "V+") {
                try {
                    const auto range = device->getIntPropertyRange(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT);
                    setVolume(device, range.cur + range.step);
                }
                catch(const ob::Error &e) {
                    std::cerr << "Volume error: " << e.what() << std::endl;
                }
                continue;
            }

            if(input == "v-" || input == "V-") {
                try {
                    const auto range = device->getIntPropertyRange(OB_PROP_DEVICE_AUDIO_OUTPUT_VOLUME_INT);
                    setVolume(device, range.cur - range.step);
                }
                catch(const ob::Error &e) {
                    std::cerr << "Volume error: " << e.what() << std::endl;
                }
                continue;
            }

            if(input.size() > 2 && (input[0] == 'v' || input[0] == 'V') && input[1] == ' ') {
                try {
                    setVolume(device, std::stoi(input.substr(2)));
                }
                catch(const ob::Error &e) {
                    std::cerr << "Volume error: " << e.what() << std::endl;
                }
                catch(const std::exception &) {
                    std::cout << "Invalid volume value. Usage: v <number>" << std::endl;
                }
                continue;
            }

            if(input.empty()) {
                continue;
            }

            std::cout << "Playing '" << input << "' ..." << std::endl;
            startPlayback(input);
        }
    }
    catch(const ob::Error &error) {
        isPlaying = false;
        if(playThread.joinable()) {
            playThread.join();
        }
        std::cerr << "SDK error: " << error.what() << " (status=" << error.getStatus() << ", type=" << error.getExceptionType() << ')' << std::endl;
        return 1;
    }
    catch(const std::exception &error) {
        isPlaying = false;
        if(playThread.joinable()) {
            playThread.join();
        }
        std::cerr << "Error: " << error.what() << std::endl;
        return 1;
    }

    return 0;
}

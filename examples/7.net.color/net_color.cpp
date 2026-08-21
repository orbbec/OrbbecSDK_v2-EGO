// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Connect to a network device by IP address and port, then stream both color channels simultaneously.
// Usage: ob_net_color [ip] [port] [h264|h265] [fps]
//   ip     - device IP address (default: 192.168.1.100)
//   port   - vendor control port (default: 8090)
//   format - encoded color format to request and record (default: h264)
//   fps    - requested frame rate; use 0 to select the device default (default: 0)
//
// The sample writes Annex-B elementary streams to net_color_left.{h264|h265}
// and net_color_right.{h264|h265}. It does not decode or re-encode frames.
// Press ESC to stop.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

const char *formatToExt(OBFormat format) {
    switch(format) {
    case OB_FORMAT_H264:
        return "h264";
    case OB_FORMAT_H265:
    case OB_FORMAT_HEVC:
        return "h265";
    default:
        return "bin";
    }
}

const char *h264NalName(uint8_t nalType) {
    switch(nalType) {
    case 1:
        return "non-IDR slice";
    case 5:
        return "IDR slice";
    case 6:
        return "SEI";
    case 7:
        return "SPS";
    case 8:
        return "PPS";
    case 9:
        return "AUD";
    default:
        return "other";
    }
}

const char *h265NalName(uint8_t nalType) {
    switch(nalType) {
    case 19:
        return "IDR_W_RADL";
    case 20:
        return "IDR_N_LP";
    case 21:
        return "CRA";
    case 32:
        return "VPS";
    case 33:
        return "SPS";
    case 34:
        return "PPS";
    case 35:
        return "AUD";
    case 39:
    case 40:
        return "SEI";
    default:
        return "other";
    }
}

size_t findStartCode(const uint8_t *data, size_t size, size_t offset, size_t *startCodeSize) {
    for(size_t i = offset; i + 3 < size; ++i) {
        if(data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1) {
            *startCodeSize = 3;
            return i;
        }
        if(i + 4 < size && data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1) {
            *startCodeSize = 4;
            return i;
        }
    }
    return size;
}

void printFirstFrameNalInfo(const char *streamName, OBFormat format, const uint8_t *data, size_t size) {
    const bool isH264 = format == OB_FORMAT_H264;
    const bool isH265 = format == OB_FORMAT_H265 || format == OB_FORMAT_HEVC;
    if(!isH264 && !isH265) {
        printf("[%s] first frame format is not H264/H265, skip NAL parse.\n", streamName);
        return;
    }

    printf("[%s] first encoded frame NALs:", streamName);
    bool   hasSlice     = false;
    bool   firstSliceIdr = false;
    size_t offset       = 0;
    int    printed      = 0;

    while(offset < size && printed < 16) {
        size_t startCodeSize = 0;
        size_t start         = findStartCode(data, size, offset, &startCodeSize);
        if(start == size) {
            break;
        }

        size_t nalOffset = start + startCodeSize;
        if(nalOffset >= size) {
            break;
        }

        uint8_t nalType = isH264 ? (data[nalOffset] & 0x1f) : ((data[nalOffset] >> 1) & 0x3f);
        printf(" %u(%s)", static_cast<unsigned>(nalType), isH264 ? h264NalName(nalType) : h265NalName(nalType));

        if(isH264 && (nalType == 1 || nalType == 5)) {
            hasSlice      = true;
            firstSliceIdr = nalType == 5;
            break;
        }
        if(isH265 && nalType <= 31) {
            hasSlice      = true;
            firstSliceIdr = nalType == 19 || nalType == 20 || nalType == 21;
            break;
        }

        offset = nalOffset + 1;
        ++printed;
    }

    if(hasSlice) {
        printf("  => first slice is %s\n", firstSliceIdr ? "IDR/I" : "not IDR/I");
    }
    else {
        printf("  => no slice NAL found in first frame\n");
    }
}

uint8_t firstSliceNalType(OBFormat format, const uint8_t *data, size_t size) {
    const bool isH264 = format == OB_FORMAT_H264;
    const bool isH265 = format == OB_FORMAT_H265 || format == OB_FORMAT_HEVC;
    if(!isH264 && !isH265) {
        return 0xff;
    }

    size_t offset = 0;
    while(offset < size) {
        size_t startCodeSize = 0;
        size_t start         = findStartCode(data, size, offset, &startCodeSize);
        if(start == size) {
            break;
        }

        size_t nalOffset = start + startCodeSize;
        if(nalOffset >= size) {
            break;
        }

        uint8_t nalType = isH264 ? (data[nalOffset] & 0x1f) : ((data[nalOffset] >> 1) & 0x3f);
        if((isH264 && (nalType == 1 || nalType == 5)) || (isH265 && nalType <= 31)) {
            return nalType;
        }

        offset = nalOffset + 1;
    }

    return 0xff;
}

const char *nalName(OBFormat format, uint8_t nalType) {
    if(format == OB_FORMAT_H264) {
        return h264NalName(nalType);
    }
    if(format == OB_FORMAT_H265 || format == OB_FORMAT_HEVC) {
        return h265NalName(nalType);
    }
    return "unknown";
}

const char *egoWorkStateToString(OBEgoWorkState state) {
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

void printEgoState(const OBEgoStateReport &state) {
    std::cout << "Ego state: protocol=" << state.protocol_version << " seq=" << state.sequence << " workState=" << egoWorkStateToString(state.work_state)
              << " stateFlags=0x" << std::hex << state.state_flags << " errorFlags=0x" << state.error_flags << std::dec
              << " storageFreeBytes=" << state.storage_free_bytes << std::endl;
}

void printImuValue(const char *name, const OBAccelValue &value, uint64_t index, uint64_t timestampUs, float temperature, const char *unit) {
    printf("[%s] index=%llu ts=%llu us temp=%.2f value=(%.6f, %.6f, %.6f) %s\n", name, (unsigned long long)index, (unsigned long long)timestampUs,
           temperature, value.x, value.y, value.z, unit);
}

class EncodedStreamRecorder {
public:
    EncodedStreamRecorder(const char *streamName, OBFormat format)
        : streamName_(streamName),
          format_(format),
          frameCount_(0),
          bytes_(0),
          minPacketSize_(std::numeric_limits<size_t>::max()),
          maxPacketSize_(0),
          droppedCount_(0),
          maxQueueDepth_(0),
          stopped_(false) {
        filePath_ = std::string("net_color_") + streamName + "." + formatToExt(format);
        file_.open(filePath_, std::ios::binary | std::ios::trunc);
        if(file_) {
            printf("[%s] recording encoded stream to %s\n", streamName_.c_str(), filePath_.c_str());
            writerThread_ = std::thread(&EncodedStreamRecorder::writerLoop, this);
        }
        else {
            printf("[%s] failed to open output file: %s\n", streamName_.c_str(), filePath_.c_str());
        }
    }

    ~EncodedStreamRecorder() {
        close();
    }

    void write(const std::shared_ptr<ob::Frame> &frame) {
        if(!file_ || !frame || !frame->getData() || frame->getDataSize() == 0) {
            return;
        }

        auto data = static_cast<const uint8_t *>(frame->getData());
        auto size = static_cast<size_t>(frame->getDataSize());

        std::vector<uint8_t> packet(data, data + size);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopped_) {
                return;
            }
            if(queue_.size() >= MAX_QUEUE_SIZE) {
                queue_.pop_front();
                ++droppedCount_;
            }
            queue_.push_back(std::move(packet));
            if(queue_.size() > maxQueueDepth_) {
                maxQueueDepth_ = queue_.size();
            }
        }
        cond_.notify_one();
    }

    void printStats() {
        uint64_t dropped       = 0;
        size_t   queueDepth    = 0;
        size_t   maxQueueDepth = 0;
        size_t   minPacketSize = 0;
        size_t   maxPacketSize = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            dropped       = droppedCount_;
            queueDepth    = queue_.size();
            maxQueueDepth = maxQueueDepth_;
            maxQueueDepth_ = queueDepth;
            minPacketSize = minPacketSize_ == std::numeric_limits<size_t>::max() ? 0 : minPacketSize_;
            maxPacketSize = maxPacketSize_;
        }

        printf("[%s] recorder queue=%zu/%zu  maxQueue=%zu  dropped=%llu  packetSize[min=%zu max=%zu]\n", streamName_.c_str(), queueDepth, MAX_QUEUE_SIZE,
               maxQueueDepth, (unsigned long long)dropped, minPacketSize, maxPacketSize);
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopped_) {
                return;
            }
            stopped_ = true;
        }
        cond_.notify_one();

        if(writerThread_.joinable()) {
            writerThread_.join();
        }

        if(file_.is_open()) {
            file_.close();
            printf("[%s] recorded %llu frames, %llu bytes, dropped %llu frames: %s\n", streamName_.c_str(), (unsigned long long)frameCount_,
                   (unsigned long long)bytes_, (unsigned long long)droppedCount_, filePath_.c_str());
        }
    }

private:
    static const size_t MAX_QUEUE_SIZE = 300;

    void writerLoop() {
        while(true) {
            std::vector<uint8_t> packet;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cond_.wait(lock, [this]() { return stopped_ || !queue_.empty(); });
                if(queue_.empty()) {
                    if(stopped_) {
                        break;
                    }
                    continue;
                }
                packet = std::move(queue_.front());
                queue_.pop_front();
            }

            if(frameCount_ == 0) {
                printFirstFrameNalInfo(streamName_.c_str(), format_, packet.data(), packet.size());
            }
            auto nalType = firstSliceNalType(format_, packet.data(), packet.size());
            if(frameCount_ < 10 || frameCount_ % 120 == 0 || packet.size() < 512) {
                printf("[%s] write frame=%llu size=%zu firstSliceNal=%u(%s)\n", streamName_.c_str(), (unsigned long long)frameCount_, packet.size(),
                       static_cast<unsigned>(nalType), nalName(format_, nalType));
            }
            file_.write(reinterpret_cast<const char *>(packet.data()), static_cast<std::streamsize>(packet.size()));
            ++frameCount_;
            bytes_ += packet.size();
            if(packet.size() < minPacketSize_) {
                minPacketSize_ = packet.size();
            }
            if(packet.size() > maxPacketSize_) {
                maxPacketSize_ = packet.size();
            }
        }
    }

    std::string               streamName_;
    OBFormat                  format_;
    std::string               filePath_;
    std::ofstream             file_;
    std::thread               writerThread_;
    std::mutex                mutex_;
    std::condition_variable   cond_;
    std::deque<std::vector<uint8_t>> queue_;
    uint64_t                  frameCount_;
    uint64_t                  bytes_;
    size_t                    minPacketSize_;
    size_t                    maxPacketSize_;
    uint64_t                  droppedCount_;
    size_t                    maxQueueDepth_;
    bool                      stopped_;
};

}  // namespace



static OBFormat parseEncodedFormat(int argc, char *argv[]) {
    if(argc <= 3) {
        return OB_FORMAT_H264;
    }

    const std::string format(argv[3]);
    if(format == "h264" || format == "H264") {
        return OB_FORMAT_H264;
    }
    if(format == "h265" || format == "H265" || format == "hevc" || format == "HEVC") {
        return OB_FORMAT_H265;
    }

    throw std::invalid_argument("format must be h264 or h265");
}

int main(int argc, char *argv[]) try {

    const char *ip          = argc > 1 ? argv[1] : "192.168.1.100";
    uint16_t    port        = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : 8090;
    OBFormat    colorFormat = parseEncodedFormat(argc, argv);
    uint32_t    colorFps    = argc > 4 ? static_cast<uint32_t>(std::strtoul(argv[4], nullptr, 10)) : OB_FPS_ANY;

    if(port == 0) {
        throw std::invalid_argument("port must be in the range 1..65535");
    }

    printf("Connecting to device at %s:%u ...\n", ip, port);

    ob::Context ctx;
    auto        device = ctx.createNetDevice(ip, port);

    auto         deviceInfo = device->getDeviceInfo();
    std::string  serialNumber = deviceInfo->getSerialNumber();
    std::cout << "Device serial number: " << serialNumber << std::endl;

    /*
    try {
        device->setEgoStateCallback([](const OBEgoStateReport &state) { printEgoState(state); });
        printf("Ego state callback registered.\n");
    }
    catch(ob::Error &e) {
        printf("Ego state unavailable: %s\n", e.what());
    }
*/
    uint8_t  sn[64] = { 0 };
    uint32_t snLen  = 0;
    device->getStructuredData(OB_STRUCT_DEVICE_SERIAL_NUMBER, sn, &snLen);


    uint8_t  devTimeData[16] = { 0 };
    uint32_t devTimeSize     = sizeof(devTimeData);
    uint64_t devTimeUs       = 0;
    device->getStructuredData(OB_STRUCT_DEVICE_TIME, devTimeData, &devTimeSize);
    memcpy(&devTimeUs, devTimeData, sizeof(devTimeUs));
    printf("Device time: %llu us\n", (unsigned long long)devTimeUs);



    ob::Pipeline pipe(device);

    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();
    // The network device currently reports only a minimal fallback profile list. Do not
    // hardcode separate left/right resolutions here: profile matching would reject the
    // right stream if firmware dimensions change. The requested encoded format is exact;
    // resolution and (unless explicitly supplied) FPS are selected from device profiles.
    config->enableVideoStream(OB_STREAM_COLOR_LEFT, OB_WIDTH_ANY, OB_HEIGHT_ANY, colorFps, colorFormat);
    config->enableVideoStream(OB_STREAM_COLOR_RIGHT, OB_WIDTH_ANY, OB_HEIGHT_ANY, colorFps, colorFormat);

    pipe.start(config);

    printf("Streams started: COLOR_LEFT and COLOR_RIGHT as raw %s. Press ESC to stop.\n", formatToExt(colorFormat));
    /*
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
                if(accelCount % 500 == 0) {
                    printImuValue("ACCEL", accelFrame->getValue(), accelFrame->getIndex(), accelFrame->getTimeStampUs(), accelFrame->getTemperature(),
                                  "m/s^2");
                }
                ++accelCount;
            }

            auto gyroFrameRaw = frameSet->getFrame(OB_FRAME_GYRO);
            if(gyroFrameRaw) {
                auto            gyroFrame = gyroFrameRaw->as<ob::GyroFrame>();
                static uint64_t gyroCount = 0;
                if(gyroCount % 500 == 0) {
                    printImuValue("GYRO", gyroFrame->getValue(), gyroFrame->getIndex(), gyroFrame->getTimeStampUs(), gyroFrame->getTemperature(), "rad/s");
                }
                ++gyroCount;
            }
        });
        printf("IMU pipeline started.\n");
    }
    catch(ob::Error &e) {
        printf("IMU unavailable: %s\n", e.what());
        imuPipe = nullptr;
    }
    */
    EncodedStreamRecorder leftRecorder("left", colorFormat);
    EncodedStreamRecorder rightRecorder("right", colorFormat);

    std::atomic<bool> running(true);

    std::thread keyThread([&]() {
        while(running) {
            char key = ob_smpl::waitForKeyPressed(100);
            if(key == ESC_KEY) {
                running = false;
            }
        }
    });

    uint64_t leftCount    = 0;
    uint64_t rightCount   = 0;
    auto     lastStatTime = std::chrono::steady_clock::now();

    while(running) {
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        auto leftFrame  = frameSet->getFrame(OB_FRAME_COLOR_LEFT);
        auto rightFrame = frameSet->getFrame(OB_FRAME_COLOR_RIGHT);

        if(leftFrame) {
            ++leftCount;
            leftRecorder.write(leftFrame);
        }
        if(rightFrame) {
           ++rightCount;
           rightRecorder.write(rightFrame);
        }

        auto now       = std::chrono::steady_clock::now();
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatTime).count();
        if(elapsedMs >= 5000) {
            double leftFps  = leftCount * 1000.0 / elapsedMs;
            double rightFps = rightCount * 1000.0 / elapsedMs;
            if(leftFrame) {
                printf("[COLOR_LEFT]  fps=%.1f  dataSize=%u bytes  ts=%llu us\n", leftFps, static_cast<unsigned>(leftFrame->getDataSize()),
                       (unsigned long long)leftFrame->getTimeStampUs());
            }
            else {
                printf("[COLOR_LEFT]  fps=%.1f  (no frame)\n", leftFps);
            }

            if(rightFrame) {
               printf("[COLOR_RIGHT] fps=%.1f  dataSize=%u bytes  ts=%llu us\n", rightFps, static_cast<unsigned>(rightFrame->getDataSize()),
                      (unsigned long long)rightFrame->getTimeStampUs());
            }
            else {
               printf("[COLOR_RIGHT] fps=%.1f  (no frame)\n", rightFps);
            }
            leftRecorder.printStats();
            rightRecorder.printStats();
            leftCount    = 0;
            rightCount   = 0;
            lastStatTime = now;
        }
    }

    printf("Stopping streams...\n");
    //if(imuPipe) {
    //    imuPipe->stop();
    //}
    pipe.stop();
    leftRecorder.close();
    rightRecorder.close();
    printf("Streams stopped.\n");

    keyThread.join();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return EXIT_FAILURE;
}
catch(const std::exception &e) {
    std::cerr << "net color sample failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

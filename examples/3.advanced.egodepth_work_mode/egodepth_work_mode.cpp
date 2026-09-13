// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char *kDefaultMode      = "Default";
constexpr const char *kFactoryCalibMode = "Factory Calib";

int gFailCount = 0;

void check(bool condition, const std::string &message) {
    if(condition) {
        std::cout << "  [PASS] " << message << std::endl;
    }
    else {
        std::cout << "  [FAIL] " << message << std::endl;
        ++gFailCount;
    }
}

std::string sensorTypeToString(OBSensorType type) {
    switch(type) {
    case OB_SENSOR_DEPTH:
        return "Depth";
    case OB_SENSOR_IR_LEFT:
        return "IR_LEFT";
    case OB_SENSOR_IR_RIGHT:
        return "IR_RIGHT";
    case OB_SENSOR_COLOR:
        return "Color";
    case OB_SENSOR_COLOR_LEFT:
        return "Color_LEFT";
    case OB_SENSOR_COLOR_RIGHT:
        return "Color_RIGHT";
    case OB_SENSOR_COLOR_TOP:
        return "Color_TOP";
    case OB_SENSOR_ACCEL:
        return "Accel";
    case OB_SENSOR_GYRO:
        return "Gyro";
    case OB_SENSOR_MAG:
        return "Mag";
    case OB_SENSOR_AUDIO:
        return "Audio";
    default:
        return "Unknown(" + std::to_string(static_cast<int>(type)) + ")";
    }
}

std::vector<OBSensorType> getSensorTypes(const std::shared_ptr<ob::Device> &device) {
    std::vector<OBSensorType> types;
    auto                      sensorList = device->getSensorList();
    if(!sensorList) {
        return types;
    }
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        types.push_back(sensorList->getSensorType(i));
    }
    return types;
}

bool hasSensor(const std::shared_ptr<ob::Device> &device, OBSensorType type) {
    auto types = getSensorTypes(device);
    return std::find(types.begin(), types.end(), type) != types.end();
}

void printSensorList(const std::shared_ptr<ob::Device> &device) {
    auto types = getSensorTypes(device);
    std::cout << "  sensor list (" << types.size() << "): ";
    for(size_t i = 0; i < types.size(); ++i) {
        if(i > 0) {
            std::cout << ", ";
        }
        std::cout << sensorTypeToString(types[i]);
    }
    std::cout << std::endl;
}

// Firmware's OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST payload is an array of packed OBEffectiveStreamProfile records.
#pragma pack(push, 1)
struct RawEffectiveStreamProfile {
    uint32_t sensorType;  // enum value of OBSensorType
    uint32_t format;      // enum value of OBFormat
    uint32_t width;
    uint32_t height;
    uint32_t maxFps;
};
#pragma pack(pop)

void printRawStreamProfileList(const std::shared_ptr<ob::Device> &device, const char *modeLabel) {
    std::cout << "raw stream profile list via OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST (" << modeLabel << "):" << std::endl;
    try {
        constexpr uint32_t kRawDataEffectiveVideoStreamProfileList = 4035;  // OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST (internal id)

        std::vector<uint8_t> data(64 * 1024);
        uint32_t             dataSize = static_cast<uint32_t>(data.size());
        ob_error            *error    = nullptr;
        ob_device_get_structure_data_list_v1_1(device->getImpl(), static_cast<OBPropertyID>(kRawDataEffectiveVideoStreamProfileList), 0, data.data(),
                                               &dataSize, &error);
        ob::Error::handle(&error);
        data.resize(dataSize);

        if(data.empty()) {
            std::cout << "  (empty)" << std::endl;
            return;
        }

        if(data.size() % sizeof(RawEffectiveStreamProfile) != 0) {
            std::cout << "  warning: raw data size " << data.size() << " is not a multiple of " << sizeof(RawEffectiveStreamProfile) << std::endl;
        }

        size_t count = 0;
        for(size_t off = 0; off + sizeof(RawEffectiveStreamProfile) <= data.size(); off += sizeof(RawEffectiveStreamProfile)) {
            RawEffectiveStreamProfile profile{};
            std::memcpy(&profile, data.data() + off, sizeof(profile));

            std::cout << "  [" << count++ << "] sensor=" << sensorTypeToString(static_cast<OBSensorType>(profile.sensorType))
                      << ", " << profile.width << "x" << profile.height << "@" << profile.maxFps
                      << ", format=" << ob::TypeHelper::convertOBFormatTypeToString(static_cast<OBFormat>(profile.format))
                      << " (" << profile.format << ")" << std::endl;
        }
    }
    catch(ob::Error &e) {
        std::cout << "  failed to fetch OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST: " << e.what() << std::endl;
    }
}

// The depth work mode switch rebuilds the device tree internally (same as 305's Dual Color switch), so the
// caller must release the old device handle and re-create it by serial number after each switch.
std::shared_ptr<ob::Device> reopenDevice(std::shared_ptr<ob::Context> &ctx, std::shared_ptr<ob::Device> &device) {
    auto sn = std::string(device->getDeviceInfo()->getSerialNumber());
    device.reset();
    auto deviceList = ctx->queryDeviceList();
    return deviceList->getDeviceBySN(sn.c_str());
}

// Start a single stream on a temporary pipeline and wait for one frame set.
bool verifyStream(const std::shared_ptr<ob::Device> &device, OBSensorType sensorType) {
    try {
        ob::Pipeline pipeline(device);
        auto         profiles = pipeline.getStreamProfileList(sensorType);
        if(!profiles || profiles->getCount() == 0) {
            std::cout << "  no stream profile for " << sensorTypeToString(sensorType) << std::endl;
            return false;
        }

        auto profile = profiles->getProfile(0);
        auto config  = std::make_shared<ob::Config>();
        config->enableStream(profile);
        pipeline.start(config);

        auto frames = pipeline.waitForFrames(5000);
        pipeline.stop();
        return frames != nullptr;
    }
    catch(ob::Error &e) {
        std::cerr << "  stream verification failed for " << sensorTypeToString(sensorType) << ": " << e.what() << std::endl;
        return false;
    }
}

}  // namespace

int main() try {
    std::cout << "=== EGODepth depth work mode (Default <-> Factory Calib) test ===" << std::endl;

    auto ctx = std::make_shared<ob::Context>();

    auto deviceList = ctx->queryDeviceList();
    if(deviceList->getCount() == 0) {
        std::cout << "No device found." << std::endl;
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    int targetIndex = -1;
    for(uint32_t i = 0; i < deviceList->getCount(); ++i) {
        std::string name = deviceList->getName(i);
        std::cout << "device[" << i << "]: " << name << std::endl;
        if(name.find("EGODepth") != std::string::npos) {
            targetIndex = static_cast<int>(i);
        }
    }

    if(targetIndex < 0) {
        std::cout << "EGODepth device not found. This sample only validates the EGODepth Default/Factory Calib switch." << std::endl;
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    auto device = deviceList->getDevice(static_cast<uint32_t>(targetIndex));
    if(!device) {
        std::cout << "Failed to create device." << std::endl;
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    if(!device->isPropertySupported(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, OB_PERMISSION_READ_WRITE)) {
        std::cout << "The device does not support depth work mode switching (firmware may not implement it yet)." << std::endl;
        ob_smpl::waitForKeyPressed();
        return -1;
    }

    auto modeList = device->getDepthWorkModeList();
    std::cout << "depth work mode list (" << modeList->getCount() << "): ";
    for(uint32_t i = 0; i < modeList->getCount(); ++i) {
        if(i > 0) {
            std::cout << ", ";
        }
        std::cout << modeList->getOBDepthWorkMode(i).name;
    }
    std::cout << std::endl;

    std::string current = device->getCurrentDepthWorkMode().name;
    std::cout << "current depth work mode: " << current << std::endl;

    // ---- Step 1: Default mode ----
    check(current == kDefaultMode, "initial mode is Default (actual: " + current + ")");
    printSensorList(device);
    check(!hasSensor(device, OB_SENSOR_IR_LEFT) && !hasSensor(device, OB_SENSOR_IR_RIGHT), "Default mode has no IR_LEFT/IR_RIGHT");
    check(hasSensor(device, OB_SENSOR_DEPTH), "Default mode has Depth");
    check(verifyStream(device, OB_SENSOR_DEPTH), "Default mode: Depth stream starts and delivers frames");

    // ---- Step 2: switch to Factory Calib ----
    std::cout << "switching depth work mode to Factory Calib..." << std::endl;
    device->switchDepthWorkMode(kFactoryCalibMode);
    device = reopenDevice(ctx, device);

    current = device->getCurrentDepthWorkMode().name;
    std::cout << "current depth work mode after switch: " << current << std::endl;
    check(current == kFactoryCalibMode, "mode after switch is Factory Calib (actual: " + current + ")");
    printSensorList(device);
    printRawStreamProfileList(device, "Factory Calib");
    check(!hasSensor(device, OB_SENSOR_DEPTH), "Factory Calib mode has no Depth");
    check(hasSensor(device, OB_SENSOR_IR_LEFT) && hasSensor(device, OB_SENSOR_IR_RIGHT), "Factory Calib mode has IR_LEFT/IR_RIGHT");
    check(verifyStream(device, OB_SENSOR_IR_LEFT), "Factory Calib mode: IR_LEFT stream starts and delivers frames");

    // ---- Step 3: switch back to Default ----
    std::cout << "switching depth work mode back to Default..." << std::endl;
    device->switchDepthWorkMode(kDefaultMode);



    device = reopenDevice(ctx, device);
    current = device->getCurrentDepthWorkMode().name;
    std::cout << "current depth work mode after switch-back: " << current << std::endl;
    check(current == kDefaultMode, "mode after switch-back is Default (actual: " + current + ")");
    printSensorList(device);
    printRawStreamProfileList(device, "Default");

    std::cout << std::endl;
    if(gFailCount == 0) {
        std::cout << "ALL CHECKS PASSED" << std::endl;
    }
    else {
        std::cout << gFailCount << " CHECK(S) FAILED" << std::endl;
    }

    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    return gFailCount == 0 ? 0 : 1;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

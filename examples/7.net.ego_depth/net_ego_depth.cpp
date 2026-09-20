// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// EgoDepth network device sample.
//
// This sample exercises two EGODepth-over-Ethernet interfaces on a device that was discovered and
// opened through createNetDevice() instead of a local USB device:
//   1. Preset (depth work mode) control: dump the available preset list, print the current preset and
//      switch the device to the "Default" work mode through ob::Device::loadPreset().
//   2. Capability discovery: enumerate the sensors the device exposes in the active work mode and
//      print every stream profile each sensor reports (stream type / format / resolution / fps).
//
// The order matters: switching the preset rebuilds the EGODepth device tree, so the sensor and
// profile list is dumped after the preset has been applied.
//
// Usage: ob_net_ego_depth [ip] [port] [preset]
//   ip     - device IP address (default: 192.168.1.100)
//   port   - vendor control port (default: 8090)
//   preset - preset / depth work mode to load (default: Default)

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

const char    *DEFAULT_IP     = "10.9.191.31";
const uint16_t DEFAULT_PORT   = 8090;
const char    *DEFAULT_PRESET = "Default";

const char *safeName(const char *name) {
    return name ? name : "<null>";
}

std::string streamTypeToString(OBStreamType type) {
    try {
        return ob::TypeHelper::convertOBStreamTypeToString(type);
    }
    catch(...) {
        return "OB_STREAM_UNKNOWN";
    }
}

std::string formatToString(OBFormat format) {
    try {
        return ob::TypeHelper::convertOBFormatTypeToString(format);
    }
    catch(...) {
        return "OB_FORMAT_UNKNOWN";
    }
}

std::string sensorTypeToString(OBSensorType type) {
    try {
        return ob::TypeHelper::convertOBSensorTypeToString(type);
    }
    catch(...) {
        return "OB_SENSOR_UNKNOWN";
    }
}

// Dump every stream profile a single sensor reports. Video profiles carry resolution and frame rate;
// the remaining profile kinds (IMU, LiDAR, ...) are printed with their format only.
void dumpStreamProfileList(const std::shared_ptr<ob::Sensor> &sensor) {
    std::shared_ptr<ob::StreamProfileList> profileList;
    try {
        profileList = sensor->getStreamProfileList();
    }
    catch(const std::exception &e) {
        printf("    stream profile list unavailable: %s\n", e.what());
        return;
    }

    if(!profileList || profileList->getCount() == 0) {
        printf("    (no stream profile reported)\n");
        return;
    }

    printf("    %u stream profile(s):\n", profileList->getCount());
    for(uint32_t i = 0; i < profileList->getCount(); ++i) {
        std::shared_ptr<ob::StreamProfile> profile;
        try {
            profile = profileList->getProfile(i);
        }
        catch(const std::exception &e) {
            printf("      [%u] <failed to read profile: %s>\n", i, e.what());
            continue;
        }

        if(!profile) {
            printf("      [%u] <null profile>\n", i);
            continue;
        }

        std::string line = "      [" + std::to_string(i) + "] " + streamTypeToString(profile->getType()) + "   " + formatToString(profile->getFormat());

        if(profile->is<ob::VideoStreamProfile>()) {
            auto videoProfile = profile->as<ob::VideoStreamProfile>();
            line += "   " + std::to_string(videoProfile->getWidth()) + "x" + std::to_string(videoProfile->getHeight()) + " @"
                    + std::to_string(videoProfile->getFps()) + "fps";
        }

        printf("%s\n", line.c_str());
    }
}

// Enumerate the sensors exposed by the device in its current work mode and print their profiles.
void dumpSensorAndProfileList(const std::shared_ptr<ob::Device> &device) {
    std::shared_ptr<ob::SensorList> sensorList;
    try {
        sensorList = device->getSensorList();
    }
    catch(const std::exception &e) {
        printf("  sensor list unavailable: %s\n", e.what());
        return;
    }

    if(!sensorList || sensorList->getCount() == 0) {
        printf("  (device reported no sensor)\n");
        return;
    }

    printf("  sensor count: %u\n", sensorList->getCount());
    for(uint32_t i = 0; i < sensorList->getCount(); ++i) {
        OBSensorType sensorType = sensorList->getSensorType(i);

        std::shared_ptr<ob::Sensor> sensor;
        try {
            sensor = sensorList->getSensor(i);
        }
        catch(const std::exception &e) {
            printf("  [%u] %s  <failed to open sensor: %s>\n", i, sensorTypeToString(sensorType).c_str(), e.what());
            continue;
        }

        printf("  [%u] %s (sensorType=%d)\n", i, sensorTypeToString(sensorType).c_str(), static_cast<int>(sensorType));

        if(sensor) {
            dumpStreamProfileList(sensor);
        }
    }
}

// Print the available preset list / current preset and load the requested preset.
bool loadPreset(const std::shared_ptr<ob::Device> &device, const std::string &presetName) {
    std::string currentPreset = safeName(device->getCurrentPresetName());

    std::shared_ptr<ob::DevicePresetList> presetList;
    try {
        presetList = device->getAvailablePresetList();
    }
    catch(const std::exception &e) {
        printf("  available preset list unavailable: %s\n", e.what());
    }

    if(presetList && presetList->getCount() > 0) {
        printf("  available preset(s): %u\n", presetList->getCount());
        for(uint32_t i = 0; i < presetList->getCount(); ++i) {
            const char *name = presetList->getName(i);
            printf("    [%u] %s%s\n", i, safeName(name), (name && currentPreset == name) ? "  <- current" : "");
        }
    }
    else {
        printf("  (device reported no available preset)\n");
    }

    printf("  current preset: %s\n", currentPreset.c_str());
    printf("  loading preset \"%s\" ...\n", presetName.c_str());

    try {
        device->loadPreset(presetName.c_str());
    }
    catch(const std::exception &e) {
        printf("  loadPreset(\"%s\") failed: %s\n", presetName.c_str(), e.what());
        return false;
    }

    std::string appliedPreset = safeName(device->getCurrentPresetName());
    printf("  current preset after load: %s\n", appliedPreset.c_str());
    return true;
}

}  // namespace

int main(int argc, char *argv[]) try {
    const char *ip     = argc > 1 ? argv[1] : DEFAULT_IP;
    uint16_t    port   = argc > 2 ? static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10)) : DEFAULT_PORT;
    std::string preset = argc > 3 ? argv[3] : DEFAULT_PRESET;

    if(port == 0) {
        throw std::invalid_argument("port must be in the range 1..65535");
    }

    printf("Connecting to EgoDepth device at %s:%u ...\n", ip, port);

    ob::Context ctx;
    auto        device = ctx.createNetDevice(ip, port);

    auto deviceInfo = device->getDeviceInfo();
    printf("Device name: %s\n", safeName(deviceInfo->getName()));
    printf("Serial number: %s\n", safeName(deviceInfo->getSerialNumber()));

    printf("\n=== 1. Preset (depth work mode) ===\n");
    bool presetApplied = loadPreset(device, preset);

    printf("\n=== 2. Supported sensor / stream profile list ===\n");
    dumpSensorAndProfileList(device);

    if(!presetApplied) {
        printf("\nPreset was not applied; the lists above reflect the device's previous work mode.\n");
    }

    printf("\nDone. Press any key to exit.");
    ob_smpl::waitForKeyPressed();

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
    std::cerr << "net ego depth sample failed: " << e.what() << std::endl;
    return EXIT_FAILURE;
}

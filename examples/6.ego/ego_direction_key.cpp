// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// At startup, choose USB or network transport. Network transport uses port 8090.
// The device reports physical key events (direction pad, power, volume, Wi-Fi handle)
// through the HID stream that is shared with the IMU. This sample registers a callback
// and prints each key event until the user quits.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr uint16_t kDefaultDevicePort = 8090;

const char *directionKeyToString(OBDirectionKey key) {
    switch(key) {
    case OB_DIRECTION_KEY_NONE:
        return "NONE";
    case OB_DIRECTION_KEY_MIDDLE:
        return "MIDDLE";
    case OB_DIRECTION_KEY_LEFT:
        return "LEFT";
    case OB_DIRECTION_KEY_RIGHT:
        return "RIGHT";
    case OB_DIRECTION_KEY_TOP:
        return "TOP";
    case OB_DIRECTION_KEY_BOTTOM:
        return "BOTTOM";
    case OB_KEY_POWER:
        return "POWER";
    case OB_KEY_VOLUME_UP:
        return "VOLUME_UP";
    case OB_KEY_VOLUME_DOWN:
        return "VOLUME_DOWN";
    case OB_KEY_WIFI_HANDLE_START:
        return "WIFI_HANDLE_START";
    case OB_KEY_WIFI_HANDLE_RESET:
        return "WIFI_HANDLE_RESET";
    default:
        return "UNKNOWN";
    }
}

const char *directionKeyStatusToString(OBDirectionKeyStatus status) {
    switch(status) {
    case OB_DIRECTION_KEY_STATUS_NONE:
        return "NONE";
    case OB_DIRECTION_KEY_STATUS_SHORT_PRESS:
        return "SHORT_PRESS";
    case OB_DIRECTION_KEY_STATUS_LONG_PRESS:
        return "LONG_PRESS";
    case OB_DIRECTION_KEY_STATUS_DOUBLE_PRESS:
        return "DOUBLE_PRESS";
    default:
        return "UNKNOWN";
    }
}

void printKeyEvent(const OBDirectionKeyInfomation &info, std::mutex &outputMutex) {
    std::lock_guard<std::mutex> lock(outputMutex);
    std::cout << "Key event: key=" << directionKeyToString(info.key) << " status=" << directionKeyStatusToString(info.status) << std::endl;
}

std::string readInput(const char *prompt) {
    std::cout << prompt;
    std::string input;
    if(!std::getline(std::cin, input)) {
        throw std::runtime_error("Unable to read console input");
    }
    return input;
}

}  // namespace

int main() try {
    ob::Context                 context;
    std::shared_ptr<ob::Device> rawDevice;
    const auto                  mode = readInput("Connection [u]sb / [n]etwork: ");
    if(mode == "u" || mode == "U") {
        auto deviceList = context.queryDeviceList();
        if(deviceList->getCount() == 0) {
            std::cerr << "No USB Ego/Wrist device found." << std::endl;
            return EXIT_FAILURE;
        }
        rawDevice = deviceList->getDevice(0);
        std::cout << "Using the first connected USB device." << std::endl;
    }
    else if(mode == "n" || mode == "N") {
        const auto ip = readInput("Enter device IP address: ");
        rawDevice     = context.createNetDevice(ip.c_str(), kDefaultDevicePort);
        std::cout << "Connected to network device " << ip << ':' << kDefaultDevicePort << std::endl;
    }
    else {
        std::cerr << "Unknown connection mode. Use u or n." << std::endl;
        return EXIT_FAILURE;
    }

    auto device = ob::DataAcquisitionDevice::fromDevice(rawDevice);

    auto outputMutex        = std::make_shared<std::mutex>();
    bool callbackRegistered = false;
    try {
        device->setDirectionKeyCallback([outputMutex](const OBDirectionKeyInfomation &info) { printKeyEvent(info, *outputMutex); });
        callbackRegistered = true;
        std::cout << "Direction key callback registered." << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Unable to register direction key callback: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Press keys on the device to see events. Press q here to quit." << std::endl;
    while(true) {
        const auto key = ob_smpl::waitForKeyPressed();
        if(key == 'q' || key == 'Q') {
            break;
        }
    }

    if(callbackRegistered) {
        device->clearDirectionKeyCallback();
    }
    return EXIT_SUCCESS;
}
catch(const ob::Error &error) {
    std::cerr << "SDK error: " << error.what() << " (status=" << error.getStatus() << ")" << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception &error) {
    std::cerr << "Error: " << error.what() << std::endl;
    return EXIT_FAILURE;
}

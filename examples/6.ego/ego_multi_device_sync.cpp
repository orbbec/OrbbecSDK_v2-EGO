// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace {

constexpr uint16_t kDefaultDevicePort = 8090;

const char *syncModeToString(OBMultiDeviceSyncMode syncMode) {
    switch(syncMode) {
    case OB_MULTI_DEVICE_SYNC_MODE_PRIMARY:
        return "PRIMARY";
    case OB_MULTI_DEVICE_SYNC_MODE_SECONDARY:
        return "SECONDARY";
    default:
        return "UNKNOWN";
    }
}

std::string readInput(const char *prompt) {
    std::cout << prompt;
    std::string input;
    std::getline(std::cin, input);
    return input;
}

void printError(const ob::Error &error) {
    std::cerr << "SDK error: " << error.what() << " (type=" << error.getExceptionType() << ')' << std::endl;
}

void printCurrentSyncMode(const std::shared_ptr<ob::Device> &device) {
    try {
        const auto syncMode = device->getMultiDeviceSyncMode();
        std::cout << "Current multi-device sync mode: " << syncModeToString(syncMode) << " (" << static_cast<uint32_t>(syncMode) << ')' << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void printSupportedSyncModes(const std::shared_ptr<ob::Device> &device) {
    try {
        const auto supportedModes = device->getSupportedMultiDeviceSyncModeBitmap();
        std::cout << "Supported sync mode bitmap: 0x" << std::hex << supportedModes << std::dec << std::endl;
        std::cout << "  PRIMARY: " << ((supportedModes & OB_MULTI_DEVICE_SYNC_MODE_PRIMARY) ? "supported" : "not supported") << std::endl;
        std::cout << "  SECONDARY: " << ((supportedModes & OB_MULTI_DEVICE_SYNC_MODE_SECONDARY) ? "supported" : "not supported") << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void setSyncMode(const std::shared_ptr<ob::Device> &device, OBMultiDeviceSyncMode syncMode) {
    try {
        device->setMultiDeviceSyncMode(syncMode);
        std::cout << "Set multi-device sync mode to " << syncModeToString(syncMode) << " (" << static_cast<uint32_t>(syncMode) << ") successfully."
                  << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void printMenu() {
    std::cout << R"(

EGO / Wrist multi-device sync test
  1  Read current sync mode
  2  Set PRIMARY mode
  3  Set SECONDARY mode
  4  Read supported sync modes
  ESC/Q  Exit
)"
              << std::endl;
}

}  // namespace

int main() try {
    ob::Context                 context;
    std::shared_ptr<ob::Device> device;

    const auto connection = readInput("Connection [u]sb / [n]etwork: ");
    if(connection == "u" || connection == "U") {
        auto deviceList = context.queryDeviceList();
        if(deviceList->getCount() == 0) {
            std::cerr << "No USB EGO/Wrist device found." << std::endl;
            return EXIT_FAILURE;
        }
        device = deviceList->getDevice(0);
        std::cout << "Using the first connected USB device." << std::endl;
    }
    else if(connection == "n" || connection == "N") {
        const auto ip = readInput("Enter device IP address: ");
        device        = context.createNetDevice(ip.c_str(), kDefaultDevicePort);
        std::cout << "Connected to network device " << ip << ':' << kDefaultDevicePort << std::endl;
    }
    else {
        std::cerr << "Unknown connection mode. Use u or n." << std::endl;
        return EXIT_FAILURE;
    }

    if(!device->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, OB_PERMISSION_READ)
       || !device->isPropertySupported(OB_STRUCT_MULTI_DEVICE_SYNC_CONFIG, OB_PERMISSION_WRITE)) {
        std::cerr << "Multi-device sync is not supported by this device or firmware (requires firmware >= 1.1.9)." << std::endl;
        return EXIT_FAILURE;
    }

    try {
        const auto supportedModes = device->getSupportedMultiDeviceSyncModeBitmap();
        const auto requiredModes  = OB_MULTI_DEVICE_SYNC_MODE_PRIMARY | OB_MULTI_DEVICE_SYNC_MODE_SECONDARY;
        if((supportedModes & requiredModes) != requiredModes) {
            std::cerr << "The device does not support both PRIMARY and SECONDARY modes. bitmap=0x" << std::hex << supportedModes << std::dec << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "Supported sync mode bitmap: 0x" << std::hex << supportedModes << std::dec << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
        return EXIT_FAILURE;
    }

    printMenu();
    while(true) {
        const char key = ob_smpl::waitForKeyPressed();
        if(key == ESC_KEY || key == 'q' || key == 'Q') {
            break;
        }

        switch(key) {
        case '1':
            printCurrentSyncMode(device);
            break;
        case '2':
            setSyncMode(device, OB_MULTI_DEVICE_SYNC_MODE_PRIMARY);
            break;
        case '3':
            setSyncMode(device, OB_MULTI_DEVICE_SYNC_MODE_SECONDARY);
            break;
        case '4':
            printSupportedSyncModes(device);
            break;
        default:
            std::cout << "Unknown key." << std::endl;
            break;
        }
    }

    return EXIT_SUCCESS;
}
catch(const ob::Error &error) {
    printError(error);
    return EXIT_FAILURE;
}

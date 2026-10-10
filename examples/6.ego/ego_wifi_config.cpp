// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Demonstrates reading and writing OB_STRUCT_WIFI_CONFIG (1124) through
// DataAcquisitionDevice. It supports either the first connected USB device or
// an Ego-series network device opened through Context::createNetDevice().

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

constexpr uint16_t kDefaultDevicePort = 8090;

void configureConsoleUtf8() {
#if defined(_WIN32)
    // Wi-Fi configuration fields are UTF-8. Match the console code pages so
    // Chinese SSIDs and passwords are displayed and entered correctly.
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string readInput(const char *prompt) {
#if defined(_WIN32)
    // waitForKeyPressed() can leave the console in non-echo, non-line-input mode.
    // Restore it before switching to std::getline for Wi-Fi credentials.
    HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  consoleMode  = 0;
    if(consoleInput != INVALID_HANDLE_VALUE && GetConsoleMode(consoleInput, &consoleMode)) {
        SetConsoleMode(consoleInput, consoleMode | ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    }
#endif
    std::cout << prompt;
    std::string input;
    if(!std::getline(std::cin, input)) {
        throw std::runtime_error("Unable to read console input");
    }
    return input;
}

void printError(const ob::Error &error) {
    std::cerr << "SDK error: " << error.what() << " (type=" << error.getExceptionType() << ')' << std::endl;
}

std::string fixedString(const char *value, size_t capacity) {
    size_t length = 0;
    while(length < capacity && value[length] != '\0') {
        ++length;
    }
    return std::string(value, length);
}

bool copyFixedString(const std::string &value, char *destination, size_t capacity, const char *fieldName) {
    if(value.size() >= capacity) {
        std::cerr << fieldName << " is too long; maximum is " << (capacity - 1) << " bytes." << std::endl;
        return false;
    }
    std::memcpy(destination, value.data(), value.size());
    return true;
}

void printWifiConfig(const OBWifiConfig &config) {
    const auto wifi     = fixedString(config.wifi, sizeof(config.wifi));
    const auto password = fixedString(config.password, sizeof(config.password));

    std::cout << "Wi-Fi: " << wifi << std::endl;
    std::cout << "Password: " << password << std::endl;
}

void readWifiConfig(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    try {
        printWifiConfig(device->getWifiConfig());
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void writeAndVerify(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    OBWifiConfig config{};
    const auto   wifi     = readInput("Wi-Fi name: ");
    const auto   password = readInput("Password: ");

    if(!copyFixedString(wifi, config.wifi, sizeof(config.wifi), "Wi-Fi name")
       || !copyFixedString(password, config.password, sizeof(config.password), "Password")) {
        return;
    }

    try {
        device->setWifiConfig(config);
        const auto readBack = device->getWifiConfig();
        if(std::memcmp(&config, &readBack, sizeof(config)) != 0) {
            std::cerr << "Wi-Fi configuration read-back does not match the written 128-byte payload." << std::endl;
            printWifiConfig(readBack);
            return;
        }

        std::cout << "Wi-Fi configuration write and read-back verification succeeded." << std::endl;
        printWifiConfig(readBack);
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void printMenu() {
    std::cout << R"(

Ego Wi-Fi configuration test
  1  Read Wi-Fi configuration
  2  Write Wi-Fi configuration and verify read-back
  ESC/Q  Exit
)"
              << std::endl;
}

}  // namespace

int main() try {
    configureConsoleUtf8();

    ob::Context                 context;
    std::shared_ptr<ob::Device> rawDevice;

    const auto connection = readInput("Connection [u]sb / [n]etwork: ");
    if(connection == "u" || connection == "U") {
        auto deviceList = context.queryDeviceList();
        if(deviceList->getCount() == 0) {
            std::cerr << "No USB Ego-series device found." << std::endl;
            return EXIT_FAILURE;
        }
        rawDevice = deviceList->getDevice(0);
        std::cout << "Using the first connected USB device." << std::endl;
    }
    else if(connection == "n" || connection == "N") {
        const auto ip = readInput("Enter device IP address: ");
        rawDevice     = context.createNetDevice(ip.c_str(), kDefaultDevicePort);
        std::cout << "Connected to network device " << ip << ':' << kDefaultDevicePort << std::endl;
    }
    else {
        std::cerr << "Unknown connection mode. Use u or n." << std::endl;
        return EXIT_FAILURE;
    }

    auto device = ob::DataAcquisitionDevice::fromDevice(rawDevice);
    if(!device->isPropertySupported(OB_STRUCT_WIFI_CONFIG, OB_PERMISSION_READ)
       || !device->isPropertySupported(OB_STRUCT_WIFI_CONFIG, OB_PERMISSION_WRITE)) {
        std::cerr << "Wi-Fi configuration (property 1124) is not supported by this device or firmware." << std::endl;
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
            readWifiConfig(device);
            break;
        case '2':
            writeAndVerify(device);
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

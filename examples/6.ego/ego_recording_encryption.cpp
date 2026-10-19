// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Demonstrates SD-card recording encryption configuration through
// DataAcquisitionDevice. The sample supports either a USB device or an
// Ego-series network device opened through Context::createNetDevice().

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

constexpr uint16_t kDefaultDevicePort = 8090;

std::string readInput(const char *prompt) {
#if defined(_WIN32)
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

std::string readPemFile(const std::string &path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if(!input) {
        throw std::runtime_error("Unable to open PEM file: " + path);
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

void readEncryptionEnabled(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    try {
        std::cout << "SD-card recording encryption: "
                  << (device->isSDCardRecordingEncryptionEnabled() ? "enabled" : "disabled") << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void writeAndVerifyEncryptionEnabled(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    const auto value = readInput("Enable encryption [0/1]: ");
    if(value != "0" && value != "1") {
        std::cerr << "Enter 0 or 1." << std::endl;
        return;
    }

    const bool expected = value == "1";
    try {
        device->setSDCardRecordingEncryptionEnabled(expected);
        const bool actual = device->isSDCardRecordingEncryptionEnabled();
        if(actual != expected) {
            std::cerr << "Encryption enable read-back mismatch." << std::endl;
            return;
        }
        std::cout << "Encryption enable write and read-back verification succeeded." << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void readPublicKey(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    try {
        const auto publicKeyPem = device->getSDCardRecordingEncryptionPublicKey();
        std::cout << "Public key size: " << publicKeyPem.size() << " bytes" << std::endl;
        std::cout << publicKeyPem;
        if(publicKeyPem.empty() || publicKeyPem.back() != '\n') {
            std::cout << std::endl;
        }
    }
    catch(const ob::Error &error) {
        printError(error);
    }
}

void writeAndVerifyPublicKey(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    const auto path = readInput("PEM public-key file path: ");
    try {
        const auto expected = readPemFile(path);
        device->setSDCardRecordingEncryptionPublicKey(expected);
        const auto actual = device->getSDCardRecordingEncryptionPublicKey();
        if(actual != expected) {
            std::cerr << "Public-key read-back mismatch." << std::endl;
            return;
        }
        std::cout << "Public-key write and read-back verification succeeded (" << actual.size() << " bytes)." << std::endl;
    }
    catch(const ob::Error &error) {
        printError(error);
    }
    catch(const std::exception &error) {
        std::cerr << error.what() << std::endl;
    }
}

void printMenu() {
    std::cout << R"(

SD-card recording encryption test
  1  Read encryption enable
  2  Set encryption enable and verify read-back
  3  Read public key
  4  Set PEM public key and verify read-back
  ESC/Q  Exit
)"
              << std::endl;
}

}  // namespace

int main() try {
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
    if(!device->isPropertySupported(OB_PROP_SD_CARD_RECORDING_ENCRYPTION_ENABLE_BOOL, OB_PERMISSION_READ_WRITE)
       || !device->isPropertySupported(OB_RAW_DATA_SD_CARD_RECORDING_ENCRYPTION_PUBLIC_KEY, OB_PERMISSION_READ_WRITE)) {
        std::cerr << "SD-card recording encryption is not supported by this device or firmware." << std::endl;
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
            readEncryptionEnabled(device);
            break;
        case '2':
            writeAndVerifyEncryptionEnabled(device);
            break;
        case '3':
            readPublicKey(device);
            break;
        case '4':
            writeAndVerifyPublicKey(device);
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
catch(const std::exception &error) {
    std::cerr << error.what() << std::endl;
    return EXIT_FAILURE;
}

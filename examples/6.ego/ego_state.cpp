// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// At startup, choose USB or network transport. Network transport uses port 8090.

#include <libobsensor/ObSensor.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr uint16_t kDefaultDevicePort = 8090;

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
    case OB_EGO_WORK_STATE_RECORD_SAVED:
        return "RECORD_SAVED";
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

void printState(const char *source, const OBEgoStateReport &state, std::mutex &outputMutex) {
    std::lock_guard<std::mutex> lock(outputMutex);
    std::cout << source << ": protocol=" << state.protocol_version << " sequence=" << state.sequence
              << " workState=" << egoWorkStateToString(state.work_state) << " stateFlags=0x" << std::hex << state.state_flags
              << " errorFlags=0x" << state.error_flags << std::dec << " storageFreeBytes=" << state.storage_free_bytes;
    if(state.recording_duration_valid) {
        std::cout << " recordingDurationMs=" << state.recording_duration_ms;
    }
    std::cout << std::endl;
}

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

}  // namespace

int main() try {
    ob::Context context;
    std::shared_ptr<ob::Device> rawDevice;
    const auto mode = readInput("Connection [u]sb / [n]etwork: ");
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
    if(!device->isPropertySupported(OB_STRUCT_EGO_STATE_REPORT, OB_PERMISSION_READ)) {
        std::cerr << "Direct Ego state query is not supported by this device or firmware." << std::endl;
        return EXIT_FAILURE;
    }

    auto outputMutex = std::make_shared<std::mutex>();
    bool callbackRegistered = false;
    try {
        device->setEgoStateCallback([outputMutex](const OBEgoStateReport &state) { printState("Callback", state, *outputMutex); });
        callbackRegistered = true;
        std::cout << "Ego state callback registered." << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Unable to register Ego state callback: " << error.what() << std::endl;
    }

    std::cout << "Commands: g = get current state directly, q = quit." << std::endl;
    for(std::string input; std::getline(std::cin, input);) {
        if(input == "q" || input == "Q") {
            break;
        }
        if(input != "g" && input != "G") {
            std::cout << "Unknown command. Use g or q." << std::endl;
            continue;
        }
        try {
            printState("Direct", device->getEgoState(), *outputMutex);
        }
        catch(const ob::Error &error) {
            std::lock_guard<std::mutex> lock(*outputMutex);
            std::cerr << "Direct state query failed: " << error.what() << std::endl;
        }
    }

    if(callbackRegistered) {
        device->clearEgoStateCallback();
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

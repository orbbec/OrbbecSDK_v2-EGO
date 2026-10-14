// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// At startup, choose USB or network transport. Network transport uses port 8090.

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
constexpr OBPropertyID kRecordingDirectoryPropertyId = OB_STRUCT_FW_RECORDING_DIRECTORY;
constexpr OBPropertyID kRecordingFileNamePropertyId  = OB_STRUCT_FW_RECORDING_FILE_NAME;
constexpr OBPropertyID kRecordingDestinationPropertyId = OB_PROP_RECORDING_DESTINATION_INT;
constexpr OBPropertyID kRecordingActionPropertyId    = OB_PROP_RECORDING_ACTION_INT;
// Temporary verification directory. Replace it with the recording task directory under test.
constexpr const char kRecordingSessionTestDirectory[] = "recording/session_test";
constexpr const char kNewRecordingSessionTestDirectory[] = "recording/new_session_test";
constexpr const char kRecordingSessionTestFileName[] = "ego_state_recording_test";

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

const char *recordingSessionStatusToString(OBRecordingSessionStatus status) {
    switch(status) {
    case OB_RECORDING_NOT_STARTED:
        return "NOT_STARTED";
    case OB_RECORDING:
        return "RECORDING";
    case OB_RECORDING_STOPPED:
        return "STOPPED";
    case OB_RECORDING_ABNORMAL:
        return "ABNORMAL";
    case OB_RECORDING_NOT_FOUND:
        return "NOT_FOUND";
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

void queryRecordingSessionResult(const std::shared_ptr<ob::DataAcquisitionDevice> &device, std::mutex &outputMutex) {
    try {
        const auto directory = device->getFwRecordingDirectory();
        const auto result = device->getRecordingSessionResult(directory);
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Recording session result: directory=" << directory << " fileExists=" << result.fileExists
                  << " recordingDuration=" << result.recordingDuration << "s"
                  << " recordingStatus=" << recordingSessionStatusToString(result.recordingStatus) << std::endl;
    }
    catch(const ob::Error &error) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Recording session result query failed: " << error.what() << std::endl;
    }
}

void setRecordingFileName(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const char *fileName, std::mutex &outputMutex) {
    if(!device->isPropertySupported(kRecordingFileNamePropertyId, OB_PERMISSION_WRITE)) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Firmware recording file name is not supported by this device or firmware." << std::endl;
        return;
    }

    try {
        device->setFwRecordingFileName(fileName);
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            std::cout << "Recording file name set to: " << fileName << std::endl;
        }
    }
    catch(const ob::Error &error) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Failed to set recording file name: " << error.what() << std::endl;
        return;
    }

    if(!device->isPropertySupported(kRecordingFileNamePropertyId, OB_PERMISSION_READ)) {
        return;
    }

    try {
        const auto readback = device->getFwRecordingFileName();
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Recording file name readback: " << readback << std::endl;
    }
    catch(const ob::Error &error) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Recording file name was set, but readback failed: " << error.what() << std::endl;
    }
}

void setRecordingTaskDirectory(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const char *directory, std::mutex &outputMutex) {
    if(!device->isPropertySupported(kRecordingDirectoryPropertyId, OB_PERMISSION_WRITE)) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Firmware recording directory is not supported by this device or firmware." << std::endl;
        return;
    }

    try {
        device->setFwRecordingDirectory(directory);
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cout << "Recording task directory set to: " << directory << std::endl;
    }
    catch(const ob::Error &error) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Failed to set recording task directory: " << error.what() << std::endl;
    }
}

void sendRecordingAction(const std::shared_ptr<ob::DataAcquisitionDevice> &device, OBRecordingAction action, std::mutex &outputMutex) {
    if(!device->isPropertySupported(kRecordingActionPropertyId, OB_PERMISSION_WRITE)) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Firmware recording action is not supported by this device or firmware." << std::endl;
        return;
    }

    try {
        if(action == OB_RECORDING_ACTION_START && device->isPropertySupported(kRecordingDestinationPropertyId, OB_PERMISSION_WRITE)) {
            device->setIntProperty(kRecordingDestinationPropertyId, static_cast<int32_t>(OB_RECORDING_DESTINATION_SD_CARD));
        }
        device->setIntProperty(kRecordingActionPropertyId, static_cast<int32_t>(action));
        std::lock_guard<std::mutex> lock(outputMutex);
        if(action == OB_RECORDING_ACTION_START) {
            std::cout << "SD-card recording start requested." << std::endl;
        }
        else {
            std::cout << "Recording stop requested." << std::endl;
        }
    }
    catch(const ob::Error &error) {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::cerr << "Failed to " << (action == OB_RECORDING_ACTION_START ? "start" : "stop") << " recording: " << error.what() << std::endl;
    }
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

void printHotkeys() {
    std::cout << R"(

Hotkeys:
  D  Set test recording directory
  N  Set new recording directory
  F  Set recording file name
  S  Start SD-card recording
  T  Stop recording
  R  Query current directory recording duration and status
  G  Get current Ego state
  Q  Quit
)" << std::endl;
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

    printHotkeys();
    while(true) {
        const auto key = ob_smpl::waitForKeyPressed();
        if(key == 'q' || key == 'Q') {
            break;
        }

        switch(key) {
        case 'd':
        case 'D':
            setRecordingTaskDirectory(device, kRecordingSessionTestDirectory, *outputMutex);
            break;
        case 'n':
        case 'N':
            setRecordingTaskDirectory(device, kNewRecordingSessionTestDirectory, *outputMutex);
            break;
        case 'f':
        case 'F':
            setRecordingFileName(device, kRecordingSessionTestFileName, *outputMutex);
            break;
        case 's':
        case 'S':
            sendRecordingAction(device, OB_RECORDING_ACTION_START, *outputMutex);
            break;
        case 't':
        case 'T':
            sendRecordingAction(device, OB_RECORDING_ACTION_STOP, *outputMutex);
            break;
        case 'g':
        case 'G':
            try {
                printState("Direct", device->getEgoState(), *outputMutex);
            }
            catch(const ob::Error &error) {
                std::lock_guard<std::mutex> lock(*outputMutex);
                std::cerr << "Direct state query failed: " << error.what() << std::endl;
            }
            break;
        case 'r':
        case 'R':
            queryRecordingSessionResult(device, *outputMutex);
            break;
        default:
            std::cout << "Unknown hotkey." << std::endl;
            printHotkeys();
            break;
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

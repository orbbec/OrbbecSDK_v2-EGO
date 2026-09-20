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

const OBPropertyID kDirectoryPropertyId   = OB_STRUCT_FW_RECORDING_DIRECTORY;
const OBPropertyID kFileNamePropertyId    = OB_STRUCT_FW_RECORDING_FILE_NAME;
const OBPropertyID kDestinationPropertyId = OB_PROP_RECORDING_DESTINATION_INT;
const OBPropertyID kActionPropertyId      = OB_PROP_RECORDING_ACTION_INT;
constexpr bool     kVerifyDirectoryReadback = true;

const char *recordingDestinationToString(int32_t destination) {
    switch(static_cast<OBRecordingDestination>(destination)) {
    case OB_RECORDING_DESTINATION_SD_CARD:
        return "SD card";
    case OB_RECORDING_DESTINATION_HOST:
        return "host computer";
    default:
        return "unknown destination";
    }
}

std::string makeMaxLengthDirectory() {
    // 240 + 1 + 239 = 480 bytes.
    // Every segment is within the firmware's 243-byte limit.
    return std::string(240, 'a') + "/" + std::string(239, 'b');
}

std::string makeTooLongDirectory() {
    return makeMaxLengthDirectory() + "d";
}

std::string makeOversizedSegmentDirectory() {
    return std::string(256, 'a');
}

void printError(const ob::Error &error) {
    std::cout << "SDK error: " << error.what() << " (type=" << error.getExceptionType() << ")" << std::endl;
}

bool setAndVerify(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const std::string &directory, const std::string &expectedDirectory, const char *name) {
    std::cout << "\n[" << name << "] setting: " << directory << std::endl;
    try {
        device->setFwRecordingDirectory(directory);
    }
    catch(const ob::Error &error) {
        std::cerr << "FAIL: set failed. ";
        printError(error);
        return false;
    }

    std::cout << "PASS: set succeeded" << std::endl;
    if(!kVerifyDirectoryReadback) {
        std::cout << "Readback disabled" << std::endl;
        return true;
    }

    std::cout << "Reading back directory..." << std::endl;
    try {
        const auto actual = device->getFwRecordingDirectory();
        if(actual != expectedDirectory) {
            std::cerr << "FAIL: readback mismatch\n  expected: " << expectedDirectory << "\n  actual:   " << actual << std::endl;
            return false;
        }
    }
    catch(const ob::Error &error) {
        std::cerr << "FAIL: readback failed. ";
        printError(error);
        return false;
    }

    std::cout << "PASS: set and readback match" << std::endl;
    return true;
}

bool setAndVerify(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const std::string &directory, const char *name) {
    return setAndVerify(device, directory, directory, name);
}

bool setAndVerifyViaStructuredData(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const std::string &directory, const char *name) {
    std::cout << "\n[" << name << "] setting through setStructuredData: " << directory << std::endl;
    try {
        device->setStructuredData(kDirectoryPropertyId, reinterpret_cast<const uint8_t *>(directory.data()), static_cast<uint32_t>(directory.size()));
    }
    catch(const ob::Error &error) {
        std::cerr << "FAIL: generic structured-data set failed. ";
        printError(error);
        return false;
    }

    std::cout << "PASS: generic structured-data set succeeded" << std::endl;
    if(!kVerifyDirectoryReadback) {
        std::cout << "Readback disabled" << std::endl;
        return true;
    }

    std::cout << "Reading back directory..." << std::endl;
    try {
        const auto actual = device->getFwRecordingDirectory();
        if(actual != directory) {
            std::cerr << "FAIL: readback mismatch\n  expected: " << directory << "\n  actual:   " << actual << std::endl;
            return false;
        }
    }
    catch(const ob::Error &error) {
        std::cerr << "FAIL: readback failed. ";
        printError(error);
        return false;
    }

    std::cout << "PASS: generic structured-data set and readback match" << std::endl;
    return true;
}

bool expectRejected(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const std::string &directory, const char *name) {
    std::cout << "\n[" << name << "] expecting rejection" << std::endl;
    try {
        device->setFwRecordingDirectory(directory);
        std::cerr << "FAIL: invalid directory was accepted" << std::endl;
        return false;
    }
    catch(const ob::Error &error) {
        std::cout << "PASS: rejected. ";
        printError(error);
        return true;
    }
}

void readCurrentDirectory(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    try {
        std::cout << "Current firmware recording directory: " << device->getFwRecordingDirectory() << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Read failed. ";
        printError(error);
    }
}

void setAndVerifyFileName(const std::shared_ptr<ob::DataAcquisitionDevice> &device, const std::string &fileName) {
    if(!device->isPropertySupported(kFileNamePropertyId, OB_PERMISSION_WRITE)) {
        std::cout << "Firmware recording file name property is not supported for writing (requires firmware >= 1.1.8)." << std::endl;
        return;
    }

    try {
        device->setFwRecordingFileName(fileName);
        std::cout << "Firmware recording file name set to: " << fileName << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to set firmware recording file name. ";
        printError(error);
        return;
    }

    if(!device->isPropertySupported(kFileNamePropertyId, OB_PERMISSION_READ)) {
        std::cout << "Firmware recording file name readback is not supported." << std::endl;
        return;
    }

    try {
        const auto actual = device->getFwRecordingFileName();
        if(actual == fileName) {
            std::cout << "File name readback matches." << std::endl;
        }
        else {
            std::cerr << "File name readback mismatch\n  expected: " << fileName << "\n  actual:   " << actual << std::endl;
        }
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to read back firmware recording file name. ";
        printError(error);
    }
}

void readCurrentFileName(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    if(!device->isPropertySupported(kFileNamePropertyId, OB_PERMISSION_READ)) {
        std::cout << "Firmware recording file name property is not supported for reading (requires firmware >= 1.1.8)." << std::endl;
        return;
    }

    try {
        std::cout << "Current firmware recording file name: " << device->getFwRecordingFileName() << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to read firmware recording file name. ";
        printError(error);
    }
}

void setRecordingDestination(const std::shared_ptr<ob::DataAcquisitionDevice> &device, OBRecordingDestination destination) {
    if(!device->isPropertySupported(kDestinationPropertyId, OB_PERMISSION_WRITE)) {
        std::cout << "Recording destination property is not supported for writing." << std::endl;
        return;
    }

    try {
        device->setIntProperty(kDestinationPropertyId, static_cast<int32_t>(destination));
        std::cout << "Recording destination set to " << recordingDestinationToString(static_cast<int32_t>(destination)) << "." << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to set recording destination. ";
        printError(error);
        return;
    }

    if(device->isPropertySupported(kDestinationPropertyId, OB_PERMISSION_READ)) {
        try {
            const auto actual = device->getIntProperty(kDestinationPropertyId);
            std::cout << "Recording destination readback: " << recordingDestinationToString(actual) << " (" << actual << ")" << std::endl;
        }
        catch(const ob::Error &error) {
            std::cerr << "Warning: recording destination was set, but readback failed. ";
            printError(error);
        }
    }
}

void readRecordingDestination(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    if(!device->isPropertySupported(kDestinationPropertyId, OB_PERMISSION_READ)) {
        std::cout << "Recording destination property is not supported for reading." << std::endl;
        return;
    }

    try {
        const auto destination = device->getIntProperty(kDestinationPropertyId);
        std::cout << "Current recording destination: " << recordingDestinationToString(destination) << " (" << destination << ")" << std::endl;
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to read recording destination. ";
        printError(error);
    }
}

void sendRecordingAction(const std::shared_ptr<ob::DataAcquisitionDevice> &device, OBRecordingAction action) {
    if(!device->isPropertySupported(kActionPropertyId, OB_PERMISSION_WRITE)) {
        std::cout << "Recording action property is not supported for writing." << std::endl;
        return;
    }

    bool    hasDestination = false;
    int32_t destination    = 0;
    if(device->isPropertySupported(kDestinationPropertyId, OB_PERMISSION_READ)) {
        try {
            destination    = device->getIntProperty(kDestinationPropertyId);
            hasDestination = true;
        }
        catch(const ob::Error &error) {
            std::cerr << "Warning: failed to read recording destination; sending the action anyway. ";
            printError(error);
        }
    }

    try {
        device->setIntProperty(kActionPropertyId, static_cast<int32_t>(action));

        std::string actionDescription = action == OB_RECORDING_ACTION_START ? "start" : "stop";
        if(hasDestination) {
            if(destination == OB_RECORDING_DESTINATION_SD_CARD) {
                std::cout << "Sent " << actionDescription << " action: SD-card recording " << actionDescription << " requested." << std::endl;
            }
            else if(destination == OB_RECORDING_DESTINATION_HOST) {
                std::cout << "Sent " << actionDescription << " action: device \"" << actionDescription << " recording\" voice prompt requested."
                          << std::endl;
            }
            else {
                std::cout << "Sent " << actionDescription << " action for destination value " << destination << "." << std::endl;
            }
        }
        else {
            std::cout << "Sent " << actionDescription << " action." << std::endl;
        }
    }
    catch(const ob::Error &error) {
        std::cerr << "Failed to send recording action. ";
        printError(error);
    }
}

void printMenu() {
    std::cout << R"(

Firmware SD-card recording directory test
  1  Valid: all_raw_data/20260703/task_20260623I001Hg_/background_00/multi_session_20260703_181212/session_003_load_20260708T072925Z_0294
  2  Valid odd-byte path: raw/session_001 (15 bytes)
  3  Valid UTF-8 path: <E4 BB BB E5 8A A1>/session_001
  4  Valid 480-byte path: 240*a/239*b
  5  Invalid empty path: ""
  6  Valid leading slash: /leading_slash/session_001 (readback: leading_slash/session_001)
  7  Invalid backslash: windows\session_001
  8  Invalid empty segment: raw//session_001
  9  Invalid current directory: raw/./session_001
  A  Invalid parent directory: raw/../session_001
  B  Valid trailing slash: raw/session_001/ (readback: raw/session_001)
  C  Invalid control character: raw/control\ncharacter
  D  Invalid UTF-8 bytes: raw/\xC3x
  E  Invalid 481-byte path: (key 4 path) + d
  F  Invalid embedded NUL: raw/nul\0character
  G  Invalid oversized segment: 256*a
  H  Valid generic setStructuredData: raw/generic_structured_data
  I  Invalid UTF-8 C1 control: raw/\xC2\x80control
  S  Valid leading/trailing slash: /leading_and_trailing/session_001/ (readback: leading_and_trailing/session_001)
  M  Expect rejection while firmware recording is active (start recording on the device first)
  R  Read current configured directory
  N  Set firmware recording file name: ego_recording_test
  K  Read current firmware recording file name
  T  Run all format tests (1-9, A-I, S; firmware recording must be stopped; changes configured directory)
  0  Set recording destination: SD card
  P  Set recording destination: host computer (action controls device voice prompts)
  L  Read current recording destination
  +  Send start action (SD card: start recording; host: play start-recording voice prompt)
  -  Send stop action (SD card: stop recording; host: play stop-recording voice prompt)
  ESC/Q  Exit
)"
              << std::endl;
}

bool runFormatTests(const std::shared_ptr<ob::DataAcquisitionDevice> &device) {
    bool passed = true;
    passed &= setAndVerify(device,
                           "all_raw_data/20260703/task_20260623I001Hg_/background_00/"
                           "multi_session_20260703_181212/session_003_load_20260708T072925Z_0294",
                           "valid customer-style path");
    passed &= setAndVerify(device, "raw/session_001", "valid odd-byte-length path");
    passed &= setAndVerify(device, std::string("\xE4\xBB\xBB\xE5\x8A\xA1") + "/session_001", "valid UTF-8 path");
    passed &= setAndVerify(device, makeMaxLengthDirectory(), "valid 480-byte path");

    passed &= expectRejected(device, "", "empty path");
    passed &= setAndVerify(device, "/leading_slash/session_001", "leading_slash/session_001", "valid leading slash path");
    passed &= expectRejected(device, "windows\\session_001", "backslash path");
    passed &= expectRejected(device, "raw//session_001", "empty path segment");
    passed &= expectRejected(device, "raw/./session_001", "current-directory segment");
    passed &= expectRejected(device, "raw/../session_001", "parent-directory segment");
    passed &= setAndVerify(device, "raw/session_001/", "raw/session_001", "valid trailing slash path");
    passed &= expectRejected(device, std::string("raw/control") + '\n' + "character", "control character");
    passed &= expectRejected(device, std::string("raw/") + "\xC3" + "x", "malformed UTF-8");
    passed &= expectRejected(device, makeTooLongDirectory(), "481-byte path");
    passed &= expectRejected(device, std::string("raw/nul") + static_cast<char>(0) + "character", "embedded NUL");
    passed &= expectRejected(device, makeOversizedSegmentDirectory(), "256-byte path segment");
    passed &= setAndVerifyViaStructuredData(device, "raw/generic_structured_data", "valid generic structured-data path");
    passed &= expectRejected(device, std::string("raw/") + "\xC2\x80" + "control", "UTF-8 C1 control character");
    passed &= setAndVerify(device, "/leading_and_trailing/session_001/", "leading_and_trailing/session_001",
                           "valid leading and trailing slash path");

    std::cout << "\nFormat test result: " << (passed ? "PASS" : "FAIL") << std::endl;
    return passed;
}

}  // namespace

int main() try {
    auto context = std::make_shared<ob::Context>();
    auto list    = context->queryDeviceList();
    if(list->getCount() == 0) {
        std::cerr << "No device found." << std::endl;
        return EXIT_FAILURE;
    }

    auto device = ob::DataAcquisitionDevice::fromDevice(list->getDevice(0));
    if(!device->isPropertySupported(kDirectoryPropertyId, OB_PERMISSION_READ) || !device->isPropertySupported(kDirectoryPropertyId, OB_PERMISSION_WRITE)) {
        std::cerr << "Firmware recording directory property is not supported by this device or firmware." << std::endl;
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
            setAndVerify(device,
                         "all_raw_data/20260703/task_20260623I001Hg_/background_00/"
                         "multi_session_20260703_181212/session_003_load_20260708T072925Z_0294",
                         "valid customer-style path");
            break;
        case '2':
            setAndVerify(device, "raw/session_001", "valid odd-byte-length path");
            break;
        case '3':
            setAndVerify(device, std::string("\xE4\xBB\xBB\xE5\x8A\xA1") + "/session_001", "valid UTF-8 path");
            break;
        case '4':
            setAndVerify(device, makeMaxLengthDirectory(), "valid 480-byte path");
            break;
        case '5':
            expectRejected(device, "", "empty path");
            break;
        case '6':
            setAndVerify(device, "/leading_slash/session_001", "leading_slash/session_001", "valid leading slash path");
            break;
        case '7':
            expectRejected(device, "windows\\session_001", "backslash path");
            break;
        case '8':
            expectRejected(device, "raw//session_001", "empty path segment");
            break;
        case '9':
            expectRejected(device, "raw/./session_001", "current-directory segment");
            break;
        case 'a':
        case 'A':
            expectRejected(device, "raw/../session_001", "parent-directory segment");
            break;
        case 'b':
        case 'B':
            setAndVerify(device, "raw/session_001/", "raw/session_001", "valid trailing slash path");
            break;
        case 'c':
        case 'C':
            expectRejected(device, std::string("raw/control") + '\n' + "character", "control character");
            break;
        case 'd':
        case 'D':
            expectRejected(device, std::string("raw/") + "\xC3" + "x", "malformed UTF-8");
            break;
        case 'e':
        case 'E':
            expectRejected(device, makeTooLongDirectory(), "481-byte path");
            break;
        case 'f':
        case 'F':
            expectRejected(device, std::string("raw/nul") + static_cast<char>(0) + "character", "embedded NUL");
            break;
        case 'g':
        case 'G':
            expectRejected(device, makeOversizedSegmentDirectory(), "256-byte path segment");
            break;
        case 'h':
        case 'H':
            setAndVerifyViaStructuredData(device, "raw/generic_structured_data", "valid generic structured-data path");
            break;
        case 'i':
        case 'I':
            expectRejected(device, std::string("raw/") + "\xC2\x80" + "control", "UTF-8 C1 control character");
            break;
        case 's':
        case 'S':
            setAndVerify(device, "/leading_and_trailing/session_001/", "leading_and_trailing/session_001",
                         "valid leading and trailing slash path");
            break;
        case 'm':
        case 'M':
            expectRejected(device, "raw/recording_active_test", "recording-active write");
            break;
        case 'r':
        case 'R':
            readCurrentDirectory(device);
            break;
        case 'n':
        case 'N':
            setAndVerifyFileName(device, "ego_recording_test");
            break;
        case 'k':
        case 'K':
            readCurrentFileName(device);
            break;
        case 't':
        case 'T':
            runFormatTests(device);
            break;
        case '0':
            setRecordingDestination(device, OB_RECORDING_DESTINATION_SD_CARD);
            break;
        case 'p':
        case 'P':
            setRecordingDestination(device, OB_RECORDING_DESTINATION_HOST);
            break;
        case 'l':
        case 'L':
            readRecordingDestination(device);
            break;
        case '+':
            sendRecordingAction(device, OB_RECORDING_ACTION_START);
            break;
        case '-':
            sendRecordingAction(device, OB_RECORDING_ACTION_STOP);
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

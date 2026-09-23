// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Network EGO device UID hot-plug verification sample.
//
// The sample opens an EGO device through createNetDevice(), records the UID
// returned by the created device, then waits for the network device removal
// callback. Disconnect the device or its network cable while it is waiting.
// It prints whether each removed device UID matches the created device UID.
// Press q to exit.
//
// Usage: ob_ego_net_uid_hotplug

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr uint16_t kDefaultPort = 8090;

std::string readDeviceIp() {
    std::cout << "Enter EGO device IP address: ";

    std::string ip;
    if(!std::getline(std::cin, ip) || ip.empty()) {
        throw std::invalid_argument("device IP address must not be empty");
    }

    return ip;
}

}  // namespace

int main() try {
    const auto ip   = readDeviceIp();
    const auto port = kDefaultPort;

    ob::Context context;

    std::mutex  mutex;
    std::string expectedUid;

    const auto callbackId = context.registerDeviceChangedCallback(
        [&](std::shared_ptr<ob::DeviceList> removedList, std::shared_ptr<ob::DeviceList>) {
            if(!removedList) {
                return;
            }

            std::lock_guard<std::mutex> lock(mutex);
            for(uint32_t i = 0; i < removedList->getCount(); ++i) {
                const char *uid = removedList->getUid(i);
                if(uid == nullptr) {
                    continue;
                }

                const std::string removedUid = uid;
                std::cout << "Device removal callback: uid=" << removedUid << std::endl;

                if(!expectedUid.empty() && removedUid == expectedUid) {
                    std::cout << "PASS: removed device UID matches the manually created device UID." << std::endl;
                    return;
                }

                std::cout << "INFO: removed device UID does not match the manually created device UID. Expected: " << expectedUid << std::endl;
            }
        });

    try {
        std::cout << "Creating network device at " << ip << ':' << port << " ..." << std::endl;
        auto device = context.createNetDevice(ip.c_str(), port);
        if(!device) {
            throw std::runtime_error("createNetDevice returned a null device");
        }

        const auto deviceInfo = device->getDeviceInfo();
        if(!deviceInfo) {
            throw std::runtime_error("created device has no device info");
        }
        const char *createdUid = deviceInfo->getUid();
        if(createdUid == nullptr || createdUid[0] == '\0') {
            throw std::runtime_error("created device has no UID");
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            expectedUid = createdUid;
        }

        std::cout << "Created device UID: " << expectedUid << std::endl;
        std::cout << "Disconnect the device or its network cable to verify the removal callback." << std::endl;
        std::cout << "The SDK debug log should contain \"Get device MAC successfully\" when the new manual MAC query path is exercised." << std::endl;
        std::cout << "Press q to exit." << std::endl;

        while(true) {
            const auto key = ob_smpl::waitForKeyPressed();
            if(key == 'q' || key == 'Q') {
                break;
            }
        }

        context.unregisterDeviceChangedCallback(callbackId);
        return EXIT_SUCCESS;
    }
    catch(...) {
        context.unregisterDeviceChangedCallback(callbackId);
        throw;
    }
}
catch(const ob::Error &error) {
    std::cerr << "EGO network UID hot-plug test failed: " << error.what() << std::endl;
    return EXIT_FAILURE;
}
catch(const std::exception &error) {
    std::cerr << "EGO network UID hot-plug test failed: " << error.what() << std::endl;
    return EXIT_FAILURE;
}

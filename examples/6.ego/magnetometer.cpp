// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Sample: read the magnetometer stream from an EGO device.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <atomic>
#include <iostream>

int main() try {
    ob::Pipeline pipe;
    auto         device = pipe.getDevice();

    auto magSensor = device->getSensor(OB_SENSOR_MAG);
    if(magSensor == nullptr) {
        std::cerr << "Device does not support the magnetometer (OB_SENSOR_MAG)." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    auto profiles = magSensor->getStreamProfileList();
    if(profiles == nullptr || profiles->getCount() == 0) {
        std::cerr << "No stream profile available for the magnetometer." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    std::atomic<uint64_t> frameCount{ 0 };
    magSensor->start(profiles->getProfile(0), [&frameCount](std::shared_ptr<ob::Frame> frame) {
        if(frame == nullptr || !frame->is<ob::MagFrame>()) {
            return;
        }

        if(frameCount++ % 50 != 0) {
            return;
        }

        auto magFrame = frame->as<ob::MagFrame>();
        auto value    = magFrame->getValue();
        std::cout << "Magnetometer: index=" << magFrame->getIndex() << ", timestamp=" << magFrame->getTimeStampUs() << " us, x=" << value.x
                  << " uT, y=" << value.y << " uT, z=" << value.z << " uT, temperature=" << magFrame->getTemperature() << " C" << std::endl;
    });

    std::cout << "Magnetometer streaming started. Press ESC to exit." << std::endl;
    while(ob_smpl::waitForKeyPressed(1) != ESC_KEY) {
    }

    magSensor->stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}

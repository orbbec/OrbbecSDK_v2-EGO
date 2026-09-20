// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Sample: read the MT6826S magnetic angle encoder stream from a UMI Finger device.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <atomic>
#include <iostream>

int main() try {
    ob::Pipeline pipe;
    auto         device = pipe.getDevice();

    auto magEncoderSensor = device->getSensor(OB_SENSOR_MAG_ENCODER);
    if(magEncoderSensor == nullptr) {
        std::cerr << "Device does not support the magnetic angle encoder (OB_SENSOR_MAG_ENCODER)." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    auto profiles = magEncoderSensor->getStreamProfileList();
    if(profiles == nullptr || profiles->getCount() == 0) {
        std::cerr << "No stream profile available for the magnetic angle encoder." << std::endl;
        std::cout << "\nPress any key to exit.";
        ob_smpl::waitForKeyPressed();
        return 0;
    }

    std::atomic<uint64_t> frameCount{ 0 };
    magEncoderSensor->start(profiles->getProfile(0), [&frameCount](std::shared_ptr<ob::Frame> frame) {
        if(frame == nullptr || frame->getDataSize() < sizeof(OBMagEncoderFrameData)) {
            return;
        }

        if(frameCount++ % 50 != 0) {
            return;
        }

        auto data = reinterpret_cast<const OBMagEncoderFrameData *>(frame->getData());
        std::cout << "MagEncoder: index=" << frame->getIndex() << ", timestamp=" << data->timestampUs << " us" << ", angle=" << data->angleDegree << " deg, status=0x" << std::hex << static_cast<int>(data->status) << std::dec
                  << " (bit0=overspeed, bit1=weak-field, bit2=undervoltage)" << std::endl;
    });

    std::cout << "Magnetic angle encoder streaming started. Press ESC to exit." << std::endl;
    while(ob_smpl::waitForKeyPressed(1) != ESC_KEY) {}

    magEncoderSensor->stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}

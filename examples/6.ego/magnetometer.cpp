// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Sample: read the magnetometer stream from an EGO device.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <chrono>
#include <iostream>

int main() try {
    ob::Pipeline pipe;
    auto config = std::make_shared<ob::Config>();
    config->enableMagStream();

    uint64_t frameCount = 0;
    auto     lastTime   = std::chrono::steady_clock::now();
    pipe.start(config, [&frameCount, &lastTime](std::shared_ptr<ob::FrameSet> frameSet) {
        if(!frameSet) {
            return;
        }

        auto frame = frameSet->getFrame(OB_FRAME_MAG);
        if(!frame || !frame->is<ob::MagFrame>()) {
            return;
        }

        ++frameCount;
        if(frameCount % 50 != 0) return;

        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - lastTime).count();
        auto fps     = elapsed > 0.0 ? 50.0 / elapsed : 0.0;
        lastTime     = now;

        auto magFrame = frame->as<ob::MagFrame>();
        auto value    = magFrame->getValue();
        std::cout << "Magnetometer: index=" << magFrame->getIndex() << ", fps=" << fps << ", timestamp=" << magFrame->getTimeStampUs() << " us, x=" << value.x
                  << " uT, y=" << value.y << " uT, z=" << value.z << " uT, temperature=" << magFrame->getTemperature() << " C" << std::endl;
    });

    std::cout << "Magnetometer pipeline streaming started. Press ESC to exit." << std::endl;
    while(ob_smpl::waitForKeyPressed(1) != ESC_KEY) {}

    pipe.stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}

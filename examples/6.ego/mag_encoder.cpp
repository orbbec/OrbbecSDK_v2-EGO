// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Sample: read the MT6826S magnetic angle encoder stream from a UMI Finger device.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <chrono>
#include <iostream>

int main() try {
    ob::Pipeline pipe;
    auto config = std::make_shared<ob::Config>();
    config->enableMagEncoderStream();

    uint64_t frameCount = 0;
    auto     lastTime   = std::chrono::steady_clock::now();
    pipe.start(config, [&frameCount, &lastTime](std::shared_ptr<ob::FrameSet> frameSet) {
        if(!frameSet) {
            return;
        }

        auto frame = frameSet->getFrame(OB_FRAME_MAG_ENCODER);
        if(!frame || frame->getDataSize() < sizeof(OBMagEncoderFrameData)) {
            return;
        }

        ++frameCount;
        if(frameCount % 50 != 0) return;

        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration<double>(now - lastTime).count();
        auto fps     = elapsed > 0.0 ? 50.0 / elapsed : 0.0;
        lastTime     = now;

        auto data = reinterpret_cast<const OBMagEncoderFrameData *>(frame->getData());
        std::cout << "MagEncoder: index=" << frame->getIndex() << ", fps=" << fps << ", timestamp=" << data->timestampUs << " us" << ", angle=" << data->angleDegree << " deg, status=0x" << std::hex << static_cast<int>(data->status) << std::dec
                  << " (bit0=overspeed, bit1=weak-field, bit2=undervoltage)" << std::endl;
    });

    std::cout << "Magnetic angle encoder pipeline streaming started. Press ESC to exit." << std::endl;
    while(ob_smpl::waitForKeyPressed(1) != ESC_KEY) {}

    pipe.stop();
    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}

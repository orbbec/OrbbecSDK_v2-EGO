// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include <libobsensor/ObSensor.hpp>

#include <iostream>
#include <string>

#include "utils.hpp"
#include "utils_opencv.hpp"

// Simple command line options for selecting which IR stream(s) to enable.
//   (no argument)            : enable Left IR only (default)
//   -r, --right              : enable Right IR only
//   -b, --both               : enable Left IR + Right IR
//   -h, --help               : print usage and exit
struct CmdOptions {
    bool enableLeftIR  = true;
    bool enableRightIR = false;
};

static void printUsage(const char *programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  -h, --help       Show this help message and exit.\n"
              << "  -r, --right      Enable the Right IR stream only (default: Left IR only).\n"
              << "  -b, --both       Enable both Left IR and Right IR streams.\n"
              << "\n"
              << "Examples:\n"
              << "  " << programName << "            # Left IR only\n"
              << "  " << programName << " -r         # Right IR only\n"
              << "  " << programName << " --both     # Left IR + Right IR\n";
}

static bool parseArguments(int argc, char *argv[], CmdOptions &opts) {
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }
        else if(arg == "-r" || arg == "--right") {
            opts.enableLeftIR  = false;
            opts.enableRightIR = true;
        }
        else if(arg == "-b" || arg == "--both") {
            opts.enableLeftIR  = true;
            opts.enableRightIR = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

int main(int argc, char *argv[]) try {
    CmdOptions opts;
    if(!parseArguments(argc, argv, opts)) {
        return 0;
    }

    // Create a pipeline with default device.
    ob::Pipeline pipe;

    // Get the device from pipeline.
    std::shared_ptr<ob::Device> device = pipe.getDevice();

    // Get the sensor list from device.
    std::shared_ptr<ob::SensorList> sensorList = device->getSensorList();

    // Create a config for pipeline.
    std::shared_ptr<ob::Config> config = std::make_shared<ob::Config>();

    // Enable the selected IR stream(s). Single-stream mode is useful for isolating
    // per-sensor throughput, e.g. on dual-camera devices.
    for(uint32_t index = 0; index < sensorList->getCount(); index++) {
        OBSensorType sensorType = sensorList->getSensorType(index);
        if((sensorType == OB_SENSOR_IR_LEFT && opts.enableLeftIR) || (sensorType == OB_SENSOR_IR_RIGHT && opts.enableRightIR)) {
            config->enableVideoStream(sensorType, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY, OB_FORMAT_ANY);
        }
    }

    pipe.start(config);

    // Create a window for rendering and set the resolution of the window
    ob_smpl::CVWindow win("Infrared", 1280, 720, ob_smpl::ARRANGE_ONE_ROW);
    while(win.run()) {
        // Wait for up to 100ms for a frameset in blocking mode.
        auto frameSet = pipe.waitForFrameset(100);
        if(frameSet == nullptr) {
            continue;
        }

        // Render a set of frame in the window.
        win.pushFramesToView(frameSet);
    }

    // Stop the pipeline, no frame data will be generated
    pipe.stop();

    return 0;
}
catch(ob::Error &e) {
    std::cerr << "function:" << e.getFunction() << "\nargs:" << e.getArgs() << "\nmessage:" << e.what() << "\nstatus:" << e.getStatus()
              << "\ntype:" << e.getExceptionType() << std::endl;
    std::cout << "\nPress any key to exit.";
    ob_smpl::waitForKeyPressed();
    exit(EXIT_FAILURE);
}

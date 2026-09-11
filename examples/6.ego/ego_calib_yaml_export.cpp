// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

// Reads the two read-only raw-data YAML blobs stored in EGO device flash and saves them to files:
//   OB_RAW_DATA_ALIGN_CALIB_YAML (4064) -> align_calib.yaml   (camera alignment calibration)
//   OB_RAW_DATA_IMU_CALIB_YAML   (4065) -> imu_calib.yaml      (IMU calibration)
// Both are transparent passthroughs: the firmware returns raw YAML bytes and the SDK does not parse
// them, so the application simply concatenates the transferred chunks and writes them out verbatim.

#include <libobsensor/ObSensor.hpp>

#include "utils.hpp"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

// Accumulate the chunked RawData transfer into one contiguous buffer.
// getRawData() delivers the payload as a sequence of OBDataChunk callbacks; each chunk carries its
// offset within the full blob, so we size the buffer from fullDataSize and copy each chunk into place.
std::vector<uint8_t> readRawData(const std::shared_ptr<ob::Device> &device, OBPropertyID propertyId) {
    std::vector<uint8_t> buffer;
    bool                 failed = false;
    std::string          errMsg;

    device->getRawData(propertyId, [&](OBDataTranState state, OBDataChunk *chunk) {
        // Negative transfer states are error codes reported by the firmware.
        if(static_cast<int>(state) < 0) {
            failed = true;
            errMsg = "transfer state " + std::to_string(static_cast<int>(state));
            return;
        }
        if(state == DATA_TRAN_STAT_TRANSFERRING && chunk != nullptr && chunk->data != nullptr) {
            if(chunk->fullDataSize > buffer.size()) {
                buffer.resize(chunk->fullDataSize);
            }
            // Guard against a malformed chunk that would write past the full-size buffer.
            if(static_cast<uint64_t>(chunk->offset) + chunk->size <= buffer.size()) {
                std::copy(chunk->data, chunk->data + chunk->size, buffer.begin() + chunk->offset);
            }
        }
    });

    if(failed) {
        throw std::runtime_error(errMsg);
    }
    return buffer;
}

// Read one raw YAML property and save it to outputPath. Returns true on success.
bool exportRawYaml(const std::shared_ptr<ob::Device> &device, OBPropertyID propertyId, const std::string &label, const std::string &outputPath) {
    if(!device->isPropertySupported(propertyId, OB_PERMISSION_READ)) {
        std::cerr << label << ": property is not supported by this device or firmware." << std::endl;
        return false;
    }

    std::vector<uint8_t> data;
    try {
        data = readRawData(device, propertyId);
    }
    catch(const ob::Error &e) {
        std::cerr << label << ": read failed: " << e.what() << std::endl;
        return false;
    }
    catch(const std::exception &e) {
        std::cerr << label << ": read failed: " << e.what() << std::endl;
        return false;
    }

    if(data.empty()) {
        std::cerr << label << ": device returned empty data." << std::endl;
        return false;
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if(!out) {
        std::cerr << label << ": failed to open output file '" << outputPath << "'." << std::endl;
        return false;
    }
    out.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    if(!out) {
        std::cerr << label << ": failed to write '" << outputPath << "'." << std::endl;
        return false;
    }

    std::cout << label << ": saved " << data.size() << " bytes to '" << outputPath << "'." << std::endl;
    return true;
}

}  // namespace

int main() try {
    auto context = std::make_shared<ob::Context>();
    auto list    = context->queryDeviceList();
    if(list->getCount() == 0) {
        std::cerr << "No device found." << std::endl;
        return EXIT_FAILURE;
    }

    auto device = list->getDevice(0);

    bool okAlign = exportRawYaml(device, OB_RAW_DATA_ALIGN_CALIB_YAML, "Align calibration YAML", "align_calib.yaml");
    bool okImu   = exportRawYaml(device, OB_RAW_DATA_IMU_CALIB_YAML, "IMU calibration YAML", "imu_calib.yaml");

    return (okAlign && okImu) ? EXIT_SUCCESS : EXIT_FAILURE;
}
catch(const ob::Error &e) {
    std::cerr << "function:" << e.getName() << "\nargs:" << e.getArgs() << "\nmessage:" << e.getMessage() << "\ntype:" << e.getExceptionType() << std::endl;
    return EXIT_FAILURE;
}

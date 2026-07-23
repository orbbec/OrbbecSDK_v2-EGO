// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace libobsensor {
struct EGOBinSegment {
    uint32_t offset = 0, size = 0;
};
struct EGOUpgradeCommand {
    enum Type { Run, UsbLoad } type = Run;
    std::string text;
    uint32_t    address = 0, offset = 0, size = 0;
};
struct EGOBinPackage {
    EGOBinSegment                  usbUpdater, uboot;
    std::vector<EGOUpgradeCommand> commands;
    uint64_t                       payloadSize = 0, totalDownloadSize = 0;
};
class EGOBinParser {
public:
    static EGOBinPackage parseAndVerify(const std::vector<uint8_t> &firmware);
};
}  // namespace libobsensor

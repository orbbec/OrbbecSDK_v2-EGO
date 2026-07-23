// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include "usb/scsi/ScsiDevicePort.hpp"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace libobsensor {
namespace egoscsi {
constexpr uint32_t DOWNLOAD_CHUNK_SIZE = 32 * 1024;
#pragma pack(push, 1)
struct LoadInfo {
    uint32_t address;
    uint32_t size;
    uint8_t  md5[16];
};
#pragma pack(pop)

class Protocol {
public:
    explicit Protocol(std::shared_ptr<IScsiDevicePort> port);
    std::array<uint8_t, 36> inquiry();
    void                    sendLoadInfo(uint32_t address, uint32_t size, const std::array<uint8_t, 16> &md5);
    void                    download(const uint8_t *data, uint32_t size, uint32_t chunkSize, const std::atomic_bool &cancelled,
                                     const std::function<void(uint32_t)> &onBytesSent = {});
    void                    runCommand(const std::string &command, bool verifyResult = true);
    void                    checkResult(uint32_t retries = 3);

private:
    void                             executeVendor(uint8_t subcode, ScsiDataDirection direction, uint8_t *data, uint32_t size, uint32_t timeoutMs = 120000);
    void                             executeVendorWithRetry(uint8_t subcode, ScsiDataDirection direction, uint8_t *data, uint32_t size, uint32_t retries = 5);
    std::shared_ptr<IScsiDevicePort> port_;
};
}  // namespace egoscsi
}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOScsiProtocol.hpp"
#include "exception/ObException.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
namespace libobsensor {
namespace egoscsi {
namespace {
enum : uint8_t { DownloadKeep = 1, GetResult = 2, DownloadEnd = 4, LoadInfoCommand = 5, RunCommand = 6 };
}
Protocol::Protocol(std::shared_ptr<IScsiDevicePort> port) : port_(std::move(port)) {
    if(!port_) {
        THROW_INVALID_PARAM_EXCEPTION("SCSI port is null");
    }
}
void Protocol::executeVendor(uint8_t subcode, ScsiDataDirection direction, uint8_t *data, uint32_t size, uint32_t timeoutMs) {
    uint8_t cdb[10]{};
    cdb[0] = 0xE8;
    cdb[1] = subcode;
    cdb[6] = static_cast<uint8_t>(size >> 24);
    cdb[7] = static_cast<uint8_t>(size >> 16);
    cdb[8] = static_cast<uint8_t>(size >> 8);
    cdb[9] = static_cast<uint8_t>(size);
    port_->execute(cdb, sizeof(cdb), direction, data, size, timeoutMs);
}
void Protocol::executeVendorWithRetry(uint8_t subcode, ScsiDataDirection direction, uint8_t *data, uint32_t size, uint32_t retries) {
    for(uint32_t attempt = 0;; ++attempt) {
        try {
            executeVendor(subcode, direction, data, size);
            return;
        }
        catch(...) {
            if(attempt >= retries) {
                throw;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }
}
std::array<uint8_t, 36> Protocol::inquiry() {
    std::array<uint8_t, 36> out{};
    uint8_t                 cdb[6] = { 0x12, 0, 0, 0, 36, 0 };
    port_->execute(cdb, sizeof(cdb), ScsiDataDirection::In, out.data(), static_cast<uint32_t>(out.size()), 2000);
    return out;
}
void Protocol::sendLoadInfo(uint32_t address, uint32_t size, const std::array<uint8_t, 16> &md5) {
    LoadInfo info{};
    info.address = address;
    info.size    = size;
    std::memcpy(info.md5, md5.data(), md5.size());
    executeVendorWithRetry(LoadInfoCommand, ScsiDataDirection::Out, reinterpret_cast<uint8_t *>(&info), sizeof(info));
}
void Protocol::download(const uint8_t *data, uint32_t size, uint32_t chunkSize, const std::atomic_bool &cancelled,
                        const std::function<void(uint32_t)> &onBytesSent) {
    if(!data || !size || !chunkSize) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid EGO download data");
    }
    uint32_t offset = 0;
    while(offset < size) {
        if(cancelled.load()) {
            THROW_IO_EXCEPTION("EGO firmware update cancelled");
        }
        auto     chunk = std::min(chunkSize, size - offset);
        uint8_t  sub   = DownloadKeep;
        if(offset + chunk == size) {
            sub = DownloadEnd;
        }
        uint32_t retry = 0;
        for(;;) {
            try {
                executeVendor(sub, ScsiDataDirection::Out, const_cast<uint8_t *>(data + offset), chunk);
                break;
            }
            catch(...) {
                if(++retry >= 5) {
                    throw;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        offset += chunk;
        if(onBytesSent) {
            onBytesSent(chunk);
        }
    }
}
void Protocol::runCommand(const std::string &command, bool verifyResult) {
    if(command.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("Empty U-Boot command");
    }

    // U-Boot tears down the USB mass-storage interface as soon as it handles
    // either command. The supplier tool treats that disconnect as expected:
    // it sends the command once and does not wait for a SCSI completion.
    const bool terminalCommand = command == "reset" || command == "poweroff";
    if(terminalCommand) {
        try {
            executeVendor(RunCommand, ScsiDataDirection::Out, reinterpret_cast<uint8_t *>(const_cast<char *>(command.data())),
                          static_cast<uint32_t>(command.size()));
        }
        catch(...) {
            // The device can reboot before Windows/Linux completes the SCSI
            // request. The command has already been submitted, so this is not
            // an upgrade failure.
        }
        return;
    }

    executeVendorWithRetry(RunCommand, ScsiDataDirection::Out, reinterpret_cast<uint8_t *>(const_cast<char *>(command.data())),
                           static_cast<uint32_t>(command.size()));
    if(verifyResult) {
        checkResult();
    }
}
void Protocol::checkResult(uint32_t retries) {
    std::array<uint8_t, 4> result{};
    do {
        result.fill(0xff);
        executeVendorWithRetry(GetResult, ScsiDataDirection::In, result.data(), static_cast<uint32_t>(result.size()), 60);
        if(result[0] == 0x0D && result[1] == 0x00) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while(retries-- > 0);
    THROW_IO_EXCEPTION("EGO bootloader reported command/download failure");
}
}  // namespace egoscsi
}  // namespace libobsensor

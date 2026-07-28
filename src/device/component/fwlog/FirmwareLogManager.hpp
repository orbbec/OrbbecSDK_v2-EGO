// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "IDevice.hpp"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace libobsensor {

using FirmwareLogPacketCallback = std::function<void(const char *fileName, OBFwLogPacketState state, const uint8_t *data, uint32_t dataSize,
                                                        uint32_t totalFileCount, uint32_t completedFileCount)>;

class FirmwareLogManager : public DeviceComponentBase {
public:
    explicit FirmwareLogManager(IDevice *owner);
    ~FirmwareLogManager() noexcept override;

    void start(FirmwareLogPacketCallback callback);
    void stop();

private:
    struct FwLogPackageHeader;
    struct FwLogFileEntry;

    bool        readAndDispatchPackage();
    std::string parseFileName(const char *fileName);
    void        workerLoop();

    std::mutex                   mutex_;
    std::thread                  worker_;
    std::atomic_bool             running_{ false };
    FirmwareLogPacketCallback callback_;
};

}  // namespace libobsensor

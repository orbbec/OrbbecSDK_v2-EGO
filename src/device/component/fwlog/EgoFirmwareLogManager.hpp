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

using EgoFirmwareLogPacketCallback = std::function<void(const char *fileName, OBEgoFwLogPacketState state, const uint8_t *data, uint32_t dataSize,
                                                        uint32_t totalFileCount, uint32_t completedFileCount)>;

class EgoFirmwareLogManager : public DeviceComponentBase {
public:
    explicit EgoFirmwareLogManager(IDevice *owner);
    ~EgoFirmwareLogManager() noexcept override;

    void start(EgoFirmwareLogPacketCallback callback);
    void stop();

private:
    struct EgoFwLogPackageHeader;
    struct EgoFwLogFileEntry;

    bool        readAndDispatchPackage();
    std::string parseFileName(const char *fileName);
    void        workerLoop();

    std::mutex                   mutex_;
    std::thread                  worker_;
    std::atomic_bool             running_{ false };
    EgoFirmwareLogPacketCallback callback_;
};

}  // namespace libobsensor

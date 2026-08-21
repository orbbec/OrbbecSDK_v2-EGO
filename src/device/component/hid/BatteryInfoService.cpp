// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "BatteryInfoService.hpp"

#include "frame/Frame.hpp"
#include "hid/HidTypes.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/alg_write_flash_crc.h"

#include <cstddef>
#include <cstring>
#include <utility>

namespace libobsensor {

namespace {

constexpr uint32_t BATTERY_INFO_MAGIC_CODE = 0x4321;

#pragma pack(push, 1)
struct BatteryInfoRaw {
    uint32_t magicCode;
    uint32_t capacity;
    uint32_t voltage;
    uint8_t  usbOnline;
    int8_t   temperature;
    int8_t   reserver[2];
    uint32_t crc32;
};
#pragma pack(pop)

static_assert(sizeof(BatteryInfoRaw) == 20, "HID battery raw group must be 20 bytes");
static_assert(sizeof(OBBatteryInfo) == 10, "HID battery public group must be 10 bytes");
static_assert(offsetof(BatteryInfoRaw, crc32) == 16, "HID battery raw crc32 offset mismatch");
static_assert(offsetof(OBBatteryInfo, capacity) == 0, "HID battery public capacity offset mismatch");
static_assert(offsetof(OBBatteryInfo, voltage) == 4, "HID battery public voltage offset mismatch");
static_assert(offsetof(OBBatteryInfo, usbOnline) == 8, "HID battery public usbOnline offset mismatch");
static_assert(offsetof(OBBatteryInfo, temperature) == 9, "HID battery public temperature offset mismatch");

uint32_t calcCrc32(uint32_t seed, const uint8_t *dataBuf, uint32_t length) {
    return alg_write_flash::CRC32(seed, const_cast<uint8_t *>(dataBuf), length);
}

uint32_t calcBatteryInfoChecksum(const OBHidHeader *header, const BatteryInfoRaw *batteryInfo) {
    auto checksum = calcCrc32(0, reinterpret_cast<const uint8_t *>(header), sizeof(OBHidHeader));
    return calcCrc32(checksum, reinterpret_cast<const uint8_t *>(batteryInfo), static_cast<uint32_t>(offsetof(BatteryInfoRaw, crc32)));
}

}  // namespace

BatteryInfoService::BatteryInfoService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend)
    : owner_(owner), backend_(backend) {}

BatteryInfoService::~BatteryInfoService() noexcept {
    bool needStop = false;
    {
        std::lock_guard<std::mutex> backendLock(backendMutex_);
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
        needStop  = running_;
    }

    if(needStop) {
        try {
            backend_->stopStream();
            std::lock_guard<std::mutex> lock(mutex_);
            running_ = false;
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop battery backend while destroying BatteryInfoService: {}", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to stop battery backend while destroying BatteryInfoService");
        }
    }
}

void BatteryInfoService::setCallback(BatteryInfoCallback callback) {
    if(!callback) {
        clearCallback();
        return;
    }

    std::lock_guard<std::mutex> backendLock(backendMutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(running_) {
            callback_ = std::move(callback);
            return;
        }
        callback_ = std::move(callback);
    }

    try {
        backend_->startStream([this](std::shared_ptr<Frame> frame) { onFrame(std::move(frame)); });
    }
    catch(...) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
        running_  = false;
        throw;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = true;
    }
}

void BatteryInfoService::clearCallback() {
    std::lock_guard<std::mutex> backendLock(backendMutex_);
    bool                    needStop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
        needStop  = running_;
    }

    if(!needStop) {
        return;
    }

    backend_->stopStream();

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

IDevice *BatteryInfoService::getOwner() const {
    return owner_;
}

void BatteryInfoService::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame) {
        return;
    }

    const auto *data     = frame->getData();
    const auto  dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    OBHidHeader header{};
    std::memcpy(&header, data, sizeof(header));
    if(header.reportId != OB_HID_REPORT_ID_BATTERY_INFOMATION_DATA || header.groupCount == 0) {
        return;
    }

    if(header.groupLen < sizeof(BatteryInfoRaw)) {
        LOG_WARN_INTVL("Battery packet is invalid, drop package! groupLen={}, minRequired={}", header.groupLen, sizeof(BatteryInfoRaw));
        return;
    }

    const auto expectedByStruct = sizeof(OBHidHeader) + sizeof(BatteryInfoRaw) * header.groupCount;
    if(dataSize < expectedByStruct) {
        LOG_WARN_INTVL("Battery packet is invalid, drop package! dataSize={}, expectedByStruct={}, groupCount={}", dataSize, expectedByStruct,
                       header.groupCount);
        return;
    }

    const auto expectedByHeader = sizeof(OBHidHeader) + static_cast<size_t>(header.groupLen) * header.groupCount;
    if(dataSize < expectedByHeader) {
        LOG_WARN_INTVL("Battery packet is invalid, drop package! dataSize={}, expectedByHeader={}, groupCount={}", dataSize, expectedByHeader, header.groupCount);
        return;
    }

    BatteryInfoCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!callback_) {
            return;
        }
        callback = callback_;
    }

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header.groupCount; ++groupIndex) {
        const auto groupOffset = static_cast<size_t>(header.groupLen) * groupIndex;
        BatteryInfoRaw raw{};
        std::memcpy(&raw, groupBase + groupOffset, sizeof(raw));

        if(raw.magicCode != BATTERY_INFO_MAGIC_CODE) {
            LOG_WARN_INTVL("Battery group is invalid, drop group! groupIndex={}, magicCode={}", groupIndex, raw.magicCode);
            continue;
        }

        const auto calcChecksum = calcBatteryInfoChecksum(&header, &raw);
        if(raw.crc32 != calcChecksum) {
            LOG_WARN_INTVL("Battery group crc32 is invalid, drop group! groupIndex={}, crc32={}, expected={}", groupIndex, raw.crc32, calcChecksum);
            continue;
        }

        OBBatteryInfo info{};
        info.capacity    = raw.capacity;
        info.voltage     = raw.voltage;
        info.usbOnline   = raw.usbOnline ? 0 : 1;
        info.temperature = raw.temperature;
        callback(info);
    }
}

}  // namespace libobsensor

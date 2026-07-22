// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DirectionKeyService.hpp"

#include "frame/Frame.hpp"
#include "hid/HidTypes.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/alg_write_flash_crc.h"

#include <cstddef>

namespace libobsensor {

namespace {

constexpr uint32_t DIRECTION_KEY_MAGIC_CODE = 0x4321;

#pragma pack(push, 1)
typedef struct {
    uint32_t magicCode;
    uint32_t eventIndex;
    uint16_t keycode;
    uint16_t pressType;
    uint32_t crc32;
} OBDirectionKeyDataRaw;
#pragma pack(pop)

uint32_t calcCrc32(uint32_t seed, const uint8_t *dataBuf, uint32_t length) {
    return alg_write_flash::CRC32(seed, const_cast<uint8_t *>(dataBuf), length);
}

uint32_t calcDirectionKeyChecksum(const OBHidHeader *header, const OBDirectionKeyDataRaw *directionData) {
    auto checksum = calcCrc32(0, reinterpret_cast<const uint8_t *>(header), sizeof(OBHidHeader));
    return calcCrc32(checksum, reinterpret_cast<const uint8_t *>(directionData), static_cast<uint32_t>(offsetof(OBDirectionKeyDataRaw, crc32)));
}

}  // namespace

DirectionKeyService::DirectionKeyService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend) : owner_(owner), backend_(backend) {}

DirectionKeyService::~DirectionKeyService() noexcept {
    bool needStop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_     = nullptr;
        needStop      = running_;
        running_      = false;
    }

    if(needStop) {
        try {
            backend_->stopStream();
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop direction key backend while destroying DirectionKeyService: {}", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to stop direction key backend while destroying DirectionKeyService");
        }
    }
}

void DirectionKeyService::setCallback(DirectionKeyCallback callback) {
    bool needStart = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = callback;
        if(running_) {
            return;
        }

        needStart = true;
    }

    if(!needStart) {
        return;
    }

    backend_->startStream([this](std::shared_ptr<Frame> frame) { onFrame(std::move(frame)); });

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
}

void DirectionKeyService::clearCallback() {
    bool needStop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
        if(!running_) {
            return;
        }
        needStop = true;
    }

    if(!needStop) {
        return;
    }

    backend_->stopStream();

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

IDevice *DirectionKeyService::getOwner() const {
    return owner_;
}

void DirectionKeyService::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame) {
        return;
    }

    const auto *data     = frame->getData();
    const auto  dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    auto header = reinterpret_cast<const OBHidHeader *>(data);
    if(header->reportId != OB_HID_REPORT_ID_DIRECTION_KEY) {
        return;
    }
    if(header->groupCount == 0) {
        return;
    }

    if(header->groupLen < sizeof(OBDirectionKeyDataRaw)) {
        LOG_WARN_INTVL("Direction key packet is invalid, drop package! groupLen={}, minRequired={}", header->groupLen, sizeof(OBDirectionKeyDataRaw));
        return;
    }

    const auto expectedByStruct = sizeof(OBHidHeader) + sizeof(OBDirectionKeyDataRaw) * header->groupCount;
    if(dataSize < expectedByStruct) {
        LOG_WARN_INTVL("Direction key packet is invalid, drop package! dataSize={}, expectedByStruct={}, groupCount={}", dataSize, expectedByStruct,
                       header->groupCount);
        return;
    }

    const auto expectedByHeader = sizeof(OBHidHeader) + static_cast<size_t>(header->groupLen) * header->groupCount;
    if(dataSize < expectedByHeader) {
        LOG_WARN_INTVL("Direction key packet is invalid, drop package! dataSize={}, expectedByHeader={}, groupCount={}", dataSize, expectedByHeader,
                       header->groupCount);
        return;
    }

    DirectionKeyCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!callback_) {
            return;
        }
        callback = callback_;
    }

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header->groupCount; ++groupIndex) {
        const auto groupOffset    = static_cast<size_t>(header->groupLen) * groupIndex;
        const auto *directionData = reinterpret_cast<const OBDirectionKeyDataRaw *>(groupBase + groupOffset);

        if(directionData->magicCode != DIRECTION_KEY_MAGIC_CODE) {
            LOG_WARN_INTVL("Direction key group is invalid, drop group! groupIndex={}, magicCode={}", groupIndex, directionData->magicCode);
            continue;
        }

        const auto calcChecksum = calcDirectionKeyChecksum(header, directionData);
        if(directionData->crc32 != calcChecksum) {
            LOG_WARN_INTVL("Direction key group crc32 is invalid, drop group! groupIndex={}, crc32={}, expected={}", groupIndex, directionData->crc32,
                           calcChecksum);
            continue;
        }

        OBDirectionKeyInfomation info{};
        info.key    = static_cast<OBDirectionKey>(directionData->keycode);
        info.status = static_cast<OBDirectionKeyStatus>(directionData->pressType);
        callback(info);
    }
}

}  // namespace libobsensor

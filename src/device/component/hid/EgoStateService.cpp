// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EgoStateService.hpp"

#include "frame/Frame.hpp"
#include "hid/HidTypes.hpp"
#include "logger/Logger.hpp"
#include "logger/LoggerInterval.hpp"
#include "utils/alg_write_flash_crc.h"

#include <cstddef>

namespace libobsensor {

namespace {

constexpr uint32_t EGO_STATE_MAGIC_CODE = 0x4321;

#pragma pack(push, 1)
typedef struct {
    uint32_t magicCode;
    uint32_t protocolVersion;
    uint32_t sequence;
    uint32_t workState;
    uint32_t stateFlags;
    uint64_t errorFlags;
    uint64_t storageFreeBytes;
    uint32_t crc32;
} OBEgoStateReportRaw;
#pragma pack(pop)

uint32_t calcCrc32(uint32_t seed, const uint8_t *dataBuf, uint32_t length) {
    return alg_write_flash::CRC32(seed, const_cast<uint8_t *>(dataBuf), length);
}

uint32_t calcEgoStateChecksum(const OBHidHeader *header, const OBEgoStateReportRaw *stateData) {
    auto checksum = calcCrc32(0, reinterpret_cast<const uint8_t *>(header), sizeof(OBHidHeader));
    return calcCrc32(checksum, reinterpret_cast<const uint8_t *>(stateData), static_cast<uint32_t>(offsetof(OBEgoStateReportRaw, crc32)));
}

}  // namespace

EgoStateService::EgoStateService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend) : owner_(owner), backend_(backend) {}

EgoStateService::~EgoStateService() noexcept {
    bool needStop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
        needStop  = running_;
        running_  = false;
    }

    if(needStop) {
        try {
            backend_->stopStream();
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop Ego state backend while destroying EgoStateService: {}", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to stop Ego state backend while destroying EgoStateService");
        }
    }
}

void EgoStateService::setCallback(EgoStateCallback callback) {
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

void EgoStateService::clearCallback() {
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

IDevice *EgoStateService::getOwner() const {
    return owner_;
}

void EgoStateService::onFrame(std::shared_ptr<Frame> frame) {
    if(!frame) {
        return;
    }

    const auto *data     = frame->getData();
    const auto  dataSize = frame->getDataSize();
    if(dataSize < sizeof(OBHidHeader)) {
        return;
    }

    auto header = reinterpret_cast<const OBHidHeader *>(data);
    if(header->reportId != OB_HID_REPORT_ID_DEVICE_STATE) {
        return;
    }
    if(header->groupCount == 0) {
        return;
    }

    if(header->groupLen < sizeof(OBEgoStateReportRaw)) {
        LOG_WARN_INTVL("Ego state packet is invalid, drop package! groupLen={}, minRequired={}", header->groupLen, sizeof(OBEgoStateReportRaw));
        return;
    }

    const auto expectedByStruct = sizeof(OBHidHeader) + sizeof(OBEgoStateReportRaw) * header->groupCount;
    if(dataSize < expectedByStruct) {
        LOG_WARN_INTVL("Ego state packet is invalid, drop package! dataSize={}, expectedByStruct={}, groupCount={}", dataSize, expectedByStruct,
                       header->groupCount);
        return;
    }

    const auto expectedByHeader = sizeof(OBHidHeader) + static_cast<size_t>(header->groupLen) * header->groupCount;
    if(dataSize < expectedByHeader) {
        LOG_WARN_INTVL("Ego state packet is invalid, drop package! dataSize={}, expectedByHeader={}, groupCount={}", dataSize, expectedByHeader,
                       header->groupCount);
        return;
    }

    EgoStateCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!callback_) {
            return;
        }
        callback = callback_;
    }

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header->groupCount; ++groupIndex) {
        const auto groupOffset = static_cast<size_t>(header->groupLen) * groupIndex;
        const auto *rawState = reinterpret_cast<const OBEgoStateReportRaw *>(groupBase + groupOffset);

        if(rawState->magicCode != EGO_STATE_MAGIC_CODE) {
            LOG_WARN_INTVL("Ego state group is invalid, drop group! groupIndex={}, magicCode={}", groupIndex, rawState->magicCode);
            continue;
        }

        const auto calcChecksum = calcEgoStateChecksum(header, rawState);
        if(rawState->crc32 != calcChecksum) {
            LOG_WARN_INTVL("Ego state group crc32 is invalid, drop group! groupIndex={}, crc32={}, expected={}", groupIndex, rawState->crc32,
                           calcChecksum);
            continue;
        }

        OBEgoStateReport state{};
        state.protocol_version   = rawState->protocolVersion;
        state.sequence           = rawState->sequence;
        state.work_state         = static_cast<OBEgoWorkState>(rawState->workState);
        state.state_flags        = rawState->stateFlags;
        state.error_flags        = rawState->errorFlags;
        state.storage_free_bytes = rawState->storageFreeBytes;
        callback(state);
    }
}

}  // namespace libobsensor

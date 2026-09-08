// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EgoStateService.hpp"

#include "IDevice.hpp"
#include "IProperty.hpp"
#include "component/fwrecord/FwRecordingPartListParser.hpp"
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

constexpr uint32_t EGO_STATE_MAGIC_CODE = 0x4321;
constexpr uint32_t EGO_STATE_PROTOCOL_VERSION_V2 = 2;

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
} OBEgoStateReportRawV1;

typedef struct {
    OBEgoStateReportRawV1 v1;
    uint64_t              recordingDurationMs;
} OBEgoStateReportRawV2;
#pragma pack(pop)

static_assert(sizeof(OBEgoStateReportRawV1) == 40, "Ego HID state v1 group must be 40 bytes");
static_assert(sizeof(OBEgoStateReportRawV2) == 48, "Ego HID state v2 group must be 48 bytes");

uint32_t calcCrc32(uint32_t seed, const uint8_t *dataBuf, uint32_t length) {
    return alg_write_flash::CRC32(seed, const_cast<uint8_t *>(dataBuf), length);
}

uint32_t calcEgoStateChecksum(const OBHidHeader *header, const OBEgoStateReportRawV1 *stateData) {
    auto checksum = calcCrc32(0, reinterpret_cast<const uint8_t *>(header), sizeof(OBHidHeader));
    return calcCrc32(checksum, reinterpret_cast<const uint8_t *>(stateData), static_cast<uint32_t>(offsetof(OBEgoStateReportRawV1, crc32)));
}

}  // namespace

EgoStateService::EgoStateService(IDevice *owner, const std::shared_ptr<IDataStreamPort> &backend) : owner_(owner), backend_(backend) {
    workerThread_ = std::thread(&EgoStateService::recordingResultWorkerLoop, this);
}

EgoStateService::~EgoStateService() noexcept {
    bool needStop = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_                = nullptr;
        recordingResultCallback_ = nullptr;
        needStop                 = running_;
        running_                 = false;
    }

    if(needStop) {
        try {
            std::lock_guard<std::mutex> backendLock(backendMutex_);
            backend_->stopStream();
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to stop Ego state backend while destroying EgoStateService: {}", e.what());
        }
        catch(...) {
            LOG_WARN("Failed to stop Ego state backend while destroying EgoStateService");
        }
    }

    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        workerExit_ = true;
        pendingRecordingResultTasks_.clear();
    }
    workerCv_.notify_one();
    if(workerThread_.joinable()) {
        workerThread_.join();
    }
}

void EgoStateService::setCallback(EgoStateCallback callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = callback;
    }
    startBackendIfNeeded();
}

void EgoStateService::clearCallback() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = nullptr;
    }
    stopBackendIfUnused();
}

OBEgoStateReport EgoStateService::getState() const {
    auto propertyServer = owner_->getPropertyServer();
    const auto data = propertyServer->getStructureData(OB_STRUCT_EGO_STATE_REPORT, PROP_ACCESS_INTERNAL);
    if(data.size() != sizeof(OBEgoStateReport)) {
        THROW_IO_EXCEPTION_WITH_ERROR("Invalid Ego state report response size", OB_ERROR_DEVICE_RESPONSE_WRONG_DATA_SIZE);
    }

    OBEgoStateReport state{};
    std::memcpy(&state, data.data(), sizeof(state));
    return state;
}

void EgoStateService::setRecordingResultCallback(EgoRecordingResultCallback callback) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recordingResultCallback_ = callback;
    }
    startBackendIfNeeded();
}

void EgoStateService::clearRecordingResultCallback() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        recordingResultCallback_ = nullptr;
    }
    stopBackendIfUnused();
}

void EgoStateService::startBackendIfNeeded() {
    std::lock_guard<std::mutex> backendLock(backendMutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(running_ || (!callback_ && !recordingResultCallback_)) {
            return;
        }
    }

    backend_->startStream([this](std::shared_ptr<Frame> frame) { onFrame(std::move(frame)); });

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
}

void EgoStateService::stopBackendIfUnused() {
    std::lock_guard<std::mutex> backendLock(backendMutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!running_ || callback_ || recordingResultCallback_) {
            return;
        }
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

    if(header->groupLen < sizeof(OBEgoStateReportRawV1)) {
        LOG_WARN_INTVL("Ego state packet is invalid, drop package! groupLen={}, minRequired={}", header->groupLen, sizeof(OBEgoStateReportRawV1));
        return;
    }

    const auto expectedByStruct = sizeof(OBHidHeader) + sizeof(OBEgoStateReportRawV1) * header->groupCount;
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

    const auto *groupBase = data + sizeof(OBHidHeader);
    for(uint8_t groupIndex = 0; groupIndex < header->groupCount; ++groupIndex) {
        const auto groupOffset = static_cast<size_t>(header->groupLen) * groupIndex;
        const auto *rawState = reinterpret_cast<const OBEgoStateReportRawV1 *>(groupBase + groupOffset);

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

        if(header->groupLen >= sizeof(OBEgoStateReportRawV2) && state.protocol_version >= EGO_STATE_PROTOCOL_VERSION_V2) {
            uint64_t recordingDurationMs = 0;
            std::memcpy(&recordingDurationMs, groupBase + groupOffset + sizeof(OBEgoStateReportRawV1), sizeof(recordingDurationMs));
            if(recordingDurationMs != 0) {
                state.recording_duration_ms    = recordingDurationMs;
                state.recording_duration_valid = true;
            }
        }

        EgoStateCallback callback;
        bool             scheduleRecordingResult = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = callback_;

            if(state.work_state == OB_EGO_WORK_STATE_RECORD_COUNTDOWN || state.work_state == OB_EGO_WORK_STATE_RECORDING) {
                processedResultSequences_.clear();
            }

            const bool recordingResultReady = state.recording_duration_valid
                                              && (state.work_state == OB_EGO_WORK_STATE_SAVING || state.work_state == OB_EGO_WORK_STATE_RECORD_SAVED
                                                  || state.work_state == OB_EGO_WORK_STATE_FAULT
                                                  || (state.error_flags & OB_EGO_ERROR_RECORD_ABORTED) != 0);
            if(recordingResultReady && recordingResultCallback_ && processedResultSequences_.insert(state.sequence).second) {
                scheduleRecordingResult = true;
            }
        }

        if(callback) {
            callback(state);
        }
        if(scheduleRecordingResult) {
            enqueueRecordingResultTask(state);
        }
    }
}

void EgoStateService::enqueueRecordingResultTask(const OBEgoStateReport &state) {
    {
        std::lock_guard<std::mutex> lock(workerMutex_);
        if(workerExit_) {
            return;
        }
        pendingRecordingResultTasks_.push_back(state);
    }
    workerCv_.notify_one();
}

void EgoStateService::recordingResultWorkerLoop() {
    while(true) {
        OBEgoStateReport state{};
        {
            std::unique_lock<std::mutex> lock(workerMutex_);
            workerCv_.wait(lock, [this]() { return workerExit_ || !pendingRecordingResultTasks_.empty(); });
            if(workerExit_) {
                return;
            }
            state = pendingRecordingResultTasks_.front();
            pendingRecordingResultTasks_.pop_front();
        }

        EgoRecordingResult result{};
        result.state = state;
        auto propertyServer = owner_->getPropertyServer();
        try {
            auto directory = propertyServer->getStructureData(OB_STRUCT_FW_RECORDING_DIRECTORY, PROP_ACCESS_INTERNAL);
            result.recordingDirectory.assign(reinterpret_cast<const char *>(directory.data()), directory.size());
            result.directoryValid = true;
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to get firmware recording result directory: {}", e.what());
        }

        try {
            auto entries = getFwRecordingPartList(owner_, PROP_ACCESS_INTERNAL);
            result.partNames.reserve(entries.size());
            for(const auto &entry : entries) {
                size_t length = 0;
                while(length < sizeof(entry.name) && entry.name[length] != '\0') {
                    ++length;
                }
                result.partNames.emplace_back(entry.name, length);
            }
            result.partListValid = true;
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to get firmware recording part list: {}", e.what());
        }

        EgoRecordingResultCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callback = recordingResultCallback_;
        }
        if(callback) {
            callback(result);
        }
    }
}

}  // namespace libobsensor

// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EgoFirmwareLogManager.hpp"

#include "property/InternalProperty.hpp"
#include "utils/alg_write_flash_crc.h"
#include "logger/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace libobsensor {

namespace {
constexpr uint32_t EGO_FW_LOG_MAGIC          = 0x4321;
constexpr uint32_t EGO_FW_LOG_NAME_MAX       = 64;
constexpr uint32_t EGO_FW_LOG_MAX_PACK_SIZE  = 512 * 1024 * 1024;
constexpr uint32_t POLL_INTERVAL_MS          = 200;
constexpr uint32_t EMPTY_POLL_INTERVAL_MS    = 500;
constexpr uint32_t ERROR_POLL_INTERVAL_MS    = 1000;

#pragma pack(push, 1)
struct EgoFwLogPackageHeaderRaw {
    uint32_t magic;
    uint16_t version;
    uint16_t headerSize;
    uint32_t fileCount;
    uint32_t totalSize;
    uint32_t crc32;
    uint32_t reserved[4];
};

struct EgoFwLogFileEntryRaw {
    char     fileName[EGO_FW_LOG_NAME_MAX];
    uint32_t fileSize;
    uint32_t fileOffset;
    uint32_t reserved[4];
};
#pragma pack(pop)

struct EgoFwLogFileState {
    std::string name;
    uint32_t    offset = 0;
    uint32_t    size   = 0;
    uint32_t    sent   = 0;
};

static_assert(sizeof(EgoFwLogPackageHeaderRaw) == 36, "Unexpected Ego firmware log package header size");
static_assert(sizeof(EgoFwLogFileEntryRaw) == 88, "Unexpected Ego firmware log file entry size");

}  // namespace

EgoFirmwareLogManager::EgoFirmwareLogManager(IDevice *owner) : DeviceComponentBase(owner) {}

EgoFirmwareLogManager::~EgoFirmwareLogManager() noexcept {
    stop();
}

void EgoFirmwareLogManager::start(EgoFirmwareLogPacketCallback callback) {
    if(!callback) {
        THROW_INVALID_PARAM_EXCEPTION("Ego firmware log callback is null");
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if(running_) {
        THROW_RESOURCE_BUSY_EXCEPTION("Ego firmware log export is already running");
    }

    callback_ = std::move(callback);
    running_  = true;
    worker_   = std::thread(&EgoFirmwareLogManager::workerLoop, this);
}

void EgoFirmwareLogManager::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!running_ && !worker_.joinable()) {
            return;
        }
        running_ = false;
    }

    if(worker_.joinable()) {
        if(worker_.get_id() == std::this_thread::get_id()) {
            worker_.detach();
        }
        else {
            worker_.join();
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = nullptr;
}

void EgoFirmwareLogManager::workerLoop() {
    while(running_) {
        try {
            if(!readAndDispatchPackage()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(EMPTY_POLL_INTERVAL_MS));
                continue;
            }

            // One successful package contains all files described by the package header.
            // Stop polling here; otherwise devices that keep the export data readable will
            // return the same package repeatedly.
            running_ = false;
        }
        catch(const std::exception &e) {
            LOG_WARN("Read Ego firmware log package failed: {}", e.what());
            std::this_thread::sleep_for(std::chrono::milliseconds(ERROR_POLL_INTERVAL_MS));
        }
    }
}

std::string EgoFirmwareLogManager::parseFileName(const char *fileName) {
    const auto end = static_cast<const char *>(memchr(fileName, '\0', EGO_FW_LOG_NAME_MAX));
    if(end == nullptr || end == fileName) {
        THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log file name");
    }

    std::string name(fileName, static_cast<size_t>(end - fileName));
    if(name.find('/') != std::string::npos || name.find('\\') != std::string::npos) {
        THROW_INVALID_DATA_EXCEPTION("Ego firmware log file name must not contain path separators");
    }
    return name;
}

bool EgoFirmwareLogManager::readAndDispatchPackage() {
    EgoFirmwareLogPacketCallback callback;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        callback = callback_;
    }
    if(!callback) {
        return false;
    }

    std::vector<uint8_t>       headerBuffer;
    EgoFwLogPackageHeaderRaw   header{};
    std::vector<EgoFwLogFileState> files;
    bool                       headerParsed = false;
    bool                       entriesParsed = false;
    bool                       receivedAnyChunk = false;
    uint32_t                   crc32 = 0;
    uint32_t                   fileDataBytes = 0;
    uint32_t                   completedFiles = 0;

    auto dispatchPacket = [&](const EgoFwLogFileState &file, OBEgoFwLogPacketState state, const uint8_t *data, uint32_t dataSize) {
        callback(file.name.c_str(), state, data, dataSize, headerParsed ? header.fileCount : 0, completedFiles);
    };

    auto parseHeaderIfReady = [&]() {
        if(headerParsed || headerBuffer.size() < sizeof(EgoFwLogPackageHeaderRaw)) {
            return;
        }

        memcpy(&header, headerBuffer.data(), sizeof(header));
        if(header.magic != EGO_FW_LOG_MAGIC) {
            THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log package magic");
        }


        if(header.totalSize > EGO_FW_LOG_MAX_PACK_SIZE) {
            THROW_INVALID_DATA_EXCEPTION("Ego firmware log package is too large");
        }

        uint32_t expectedHeaderSize = static_cast<uint32_t>(sizeof(EgoFwLogPackageHeaderRaw) + header.fileCount * sizeof(EgoFwLogFileEntryRaw));
        if(header.headerSize != expectedHeaderSize || header.headerSize > header.totalSize) {
            THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log package header");
        }
        headerParsed = true;
    };

    auto parseEntriesIfReady = [&]() {
        if(entriesParsed || !headerParsed || headerBuffer.size() < header.headerSize) {
            return;
        }

        const auto *entries = reinterpret_cast<const EgoFwLogFileEntryRaw *>(headerBuffer.data() + sizeof(EgoFwLogPackageHeaderRaw));
        files.reserve(header.fileCount);
        uint32_t expectedOffset = header.headerSize;
        for(uint32_t i = 0; i < header.fileCount; i++) {
            EgoFwLogFileState file;
            file.name   = parseFileName(entries[i].fileName);
            file.offset = entries[i].fileOffset;
            file.size   = entries[i].fileSize;

            if(file.offset != expectedOffset || file.size > header.totalSize - file.offset) {
                THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log file range");
            }

            expectedOffset += file.size;
            files.emplace_back(std::move(file));
        }

        if(expectedOffset != header.totalSize) {
            THROW_INVALID_DATA_EXCEPTION("Ego firmware log file data is not continuous");
        }

        for(auto &file: files) {
            if(file.size == 0) {
                dispatchPacket(file, EGO_FW_LOG_PACKET_BEGIN, nullptr, 0);
                completedFiles++;
                dispatchPacket(file, EGO_FW_LOG_PACKET_END, nullptr, 0);
            }
        }
        entriesParsed = true;
    };

    auto dispatchFileData = [&](uint32_t chunkOffset, const uint8_t *chunkData, uint32_t chunkSize) {
        if(!entriesParsed || chunkSize == 0 || chunkData == nullptr) {
            return;
        }

        uint32_t chunkEnd = chunkOffset + chunkSize;
        uint32_t crcBegin = std::max(chunkOffset, static_cast<uint32_t>(header.headerSize));
        uint32_t crcEnd   = std::min(chunkEnd, header.totalSize);
        if(crcBegin < crcEnd) {
            crc32 = alg_write_flash::CRC32(crc32, const_cast<uint8_t *>(chunkData + (crcBegin - chunkOffset)), crcEnd - crcBegin);
            fileDataBytes += crcEnd - crcBegin;
        }

        for(auto &file: files) {
            if(file.size == 0 || file.sent >= file.size) {
                continue;
            }

            uint32_t fileBegin = file.offset;
            uint32_t fileEnd   = file.offset + file.size;
            uint32_t begin     = std::max(chunkOffset, fileBegin);
            uint32_t end       = std::min(chunkEnd, fileEnd);
            if(begin >= end) {
                continue;
            }

            uint32_t packetSize = end - begin;
            const uint8_t *data = chunkData + (begin - chunkOffset);
            if(begin != file.offset + file.sent) {
                THROW_INVALID_DATA_EXCEPTION("Ego firmware log file data is not continuous");
            }

            auto state = EGO_FW_LOG_PACKET_DATA;
            if(file.sent == 0) {
                state = EGO_FW_LOG_PACKET_BEGIN;
            }
            bool fileCompleted = file.sent + packetSize == file.size;
            if(fileCompleted && file.sent != 0) {
                state = EGO_FW_LOG_PACKET_END;
                completedFiles++;
            }

            dispatchPacket(file, state, data, packetSize);
            file.sent += packetSize;

            if(fileCompleted && state != EGO_FW_LOG_PACKET_END) {
                completedFiles++;
                dispatchPacket(file, EGO_FW_LOG_PACKET_END, nullptr, 0);
            }
        }
    };

    auto propServer = getOwner()->getPropertyServer();
    propServer->getRawData(
        OB_RAW_DATA_EXPORT_LOG,
        [&](OBDataTranState state, OBDataChunk *dataChunk) {
            if(!running_) {
                return;
            }
            if(state < 0) {
                THROW_IO_EXCEPTION("Read Ego firmware log raw data failed");
            }
            if(state != DATA_TRAN_STAT_TRANSFERRING) {
                return;
            }
            if(dataChunk == nullptr || (dataChunk->size > 0 && dataChunk->data == nullptr)) {
                THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log raw data chunk");
            }
            if(dataChunk->offset > dataChunk->fullDataSize || dataChunk->size > dataChunk->fullDataSize - dataChunk->offset) {
                THROW_INVALID_DATA_EXCEPTION("Invalid Ego firmware log raw data chunk range");
            }
            if(headerParsed && dataChunk->fullDataSize != header.totalSize) {
                THROW_INVALID_DATA_EXCEPTION("Ego firmware log raw data size changed");
            }

            receivedAnyChunk = true;

            uint32_t chunkOffset = dataChunk->offset;
            uint32_t chunkSize   = dataChunk->size;
            uint32_t chunkEnd    = chunkOffset + chunkSize;

            if(!entriesParsed) {
                uint32_t bytesNeeded = headerParsed ? header.headerSize : static_cast<uint32_t>(sizeof(EgoFwLogPackageHeaderRaw));
                uint32_t copyEnd     = std::min(chunkEnd, bytesNeeded);
                if(chunkOffset < copyEnd) {
                    if(headerBuffer.size() != chunkOffset) {
                        THROW_INVALID_DATA_EXCEPTION("Ego firmware log header chunks are not contiguous");
                    }
                    headerBuffer.insert(headerBuffer.end(), dataChunk->data, dataChunk->data + (copyEnd - chunkOffset));
                    parseHeaderIfReady();
                }

                if(headerParsed && headerBuffer.size() < header.headerSize && chunkEnd > headerBuffer.size()) {
                    uint32_t begin = std::max(chunkOffset, static_cast<uint32_t>(headerBuffer.size()));
                    uint32_t end   = std::min(chunkEnd, static_cast<uint32_t>(header.headerSize));
                    if(begin < end) {
                        if(headerBuffer.size() != begin) {
                            THROW_INVALID_DATA_EXCEPTION("Ego firmware log entry chunks are not contiguous");
                        }
                        headerBuffer.insert(headerBuffer.end(), dataChunk->data + (begin - chunkOffset), dataChunk->data + (end - chunkOffset));
                    }
                }

                parseEntriesIfReady();
            }

            dispatchFileData(chunkOffset, dataChunk->data, chunkSize);
        },
        PROP_ACCESS_USER);

    if(!receivedAnyChunk) {
        return false;
    }
    if(!entriesParsed) {
        THROW_INVALID_DATA_EXCEPTION("Incomplete Ego firmware log package header");
    }
    for(const auto &file: files) {
        if(file.sent != file.size) {
            THROW_INVALID_DATA_EXCEPTION("Incomplete Ego firmware log file data");
        }
    }
    if(fileDataBytes != header.totalSize - header.headerSize) {
        THROW_INVALID_DATA_EXCEPTION("Incomplete Ego firmware log data section");
    }
    if(header.crc32 != 0 && crc32 != header.crc32) {
        THROW_INVALID_DATA_EXCEPTION("Ego firmware log package CRC32 mismatch");
    }
    return true;
}

}  // namespace libobsensor

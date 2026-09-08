// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "RecordingSessionResultManager.hpp"

#include "exception/ObException.hpp"
#include "libobsensor/h/Property.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace libobsensor {
namespace {

constexpr size_t kStoragePathMaxLength        = 480;
constexpr size_t kStoragePathSegmentMaxLength = 255;
constexpr size_t kPathPayloadHeaderSize       = sizeof(uint16_t);
constexpr size_t kResultPayloadSize           = sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint8_t);

uint32_t readUint32LE(const uint8_t *data) {
    uint32_t value = 0;
    for(size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<uint32_t>(data[index]) << (index * 8);
    }
    return value;
}

uint64_t readUint64LE(const uint8_t *data) {
    uint64_t value = 0;
    for(size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<uint64_t>(data[index]) << (index * 8);
    }
    return value;
}

}  // namespace

RecordingSessionResultManager::RecordingSessionResultManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor)
    : DeviceComponentBase(owner), vendorPropertyAccessor_(std::move(vendorPropertyAccessor)) {
    if(!vendorPropertyAccessor_) {
        THROW_INVALID_PARAM_EXCEPTION("Recording session result vendor property accessor is null");
    }
}

OBRecordingSessionResult RecordingSessionResultManager::getRecordingSessionResult(const std::string &storagePath) {
    std::vector<uint8_t> path(storagePath.begin(), storagePath.end());
    validateStoragePath(path);

    std::lock_guard<std::mutex> lock(queryMutex_);
    vendorPropertyAccessor_->setStructureData(OB_STRUCT_RECORDING_SESSION_RESULT, buildPathPayload(path));
    return parseResultPayload(vendorPropertyAccessor_->getStructureData(OB_STRUCT_RECORDING_SESSION_RESULT));
}

void RecordingSessionResultManager::validateStoragePath(const std::vector<uint8_t> &storagePath) {
    if(storagePath.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: path must not be empty");
    }
    if(storagePath.size() > kStoragePathMaxLength) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: path exceeds 480 bytes");
    }
    if(!isValidUtf8(storagePath)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: path must be valid UTF-8");
    }

    for(size_t index = 0; index < storagePath.size(); ++index) {
        const auto value = storagePath[index];
        if(value == 0 || value < 0x20 || value == 0x7F || value == '\\') {
            THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: control character or backslash is not allowed");
        }
        if(value == 0xC2 && index + 1 < storagePath.size() && storagePath[index + 1] >= 0x80 && storagePath[index + 1] <= 0x9F) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: control character is not allowed");
        }
    }

    size_t segmentBegin = storagePath.front() == '/' ? 1 : 0;
    while(segmentBegin < storagePath.size()) {
        const auto separator = std::find(storagePath.begin() + segmentBegin, storagePath.end(), static_cast<uint8_t>('/'));
        const auto segmentEnd = static_cast<size_t>(std::distance(storagePath.begin(), separator));
        const auto segmentLength = segmentEnd - segmentBegin;
        if(segmentLength == 0) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: empty path segment is not allowed");
        }
        if(segmentLength > kStoragePathSegmentMaxLength) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: path segment exceeds 255 bytes");
        }

        const std::string segment(reinterpret_cast<const char *>(storagePath.data() + segmentBegin), segmentLength);
        if(segment == "." || segment == "..") {
            THROW_INVALID_PARAM_EXCEPTION("Invalid recording session storage path: '.' and '..' are not allowed");
        }
        if(separator == storagePath.end()) {
            break;
        }
        segmentBegin = segmentEnd + 1;
    }
}

bool RecordingSessionResultManager::isValidUtf8(const std::vector<uint8_t> &data) {
    for(size_t index = 0; index < data.size();) {
        const auto first = data[index];
        if(first <= 0x7F) {
            ++index;
            continue;
        }

        size_t sequenceLength = 0;
        uint32_t codePoint = 0;
        uint32_t minimumValue = 0;
        if((first & 0xE0) == 0xC0) {
            sequenceLength = 2;
            codePoint = first & 0x1F;
            minimumValue = 0x80;
        }
        else if((first & 0xF0) == 0xE0) {
            sequenceLength = 3;
            codePoint = first & 0x0F;
            minimumValue = 0x800;
        }
        else if((first & 0xF8) == 0xF0) {
            sequenceLength = 4;
            codePoint = first & 0x07;
            minimumValue = 0x10000;
        }
        else {
            return false;
        }

        if(index + sequenceLength > data.size()) {
            return false;
        }
        for(size_t offset = 1; offset < sequenceLength; ++offset) {
            const auto value = data[index + offset];
            if((value & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (value & 0x3F);
        }
        if(codePoint < minimumValue || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
            return false;
        }
        index += sequenceLength;
    }
    return true;
}

std::vector<uint8_t> RecordingSessionResultManager::buildPathPayload(const std::vector<uint8_t> &storagePath) {
    const auto pathLength = static_cast<uint16_t>(storagePath.size());
    std::vector<uint8_t> payload(kPathPayloadHeaderSize + storagePath.size());
    payload[0] = static_cast<uint8_t>(pathLength & 0xFF);
    payload[1] = static_cast<uint8_t>((pathLength >> 8) & 0xFF);
    std::memcpy(payload.data() + kPathPayloadHeaderSize, storagePath.data(), storagePath.size());
    return payload;
}

OBRecordingSessionResult RecordingSessionResultManager::parseResultPayload(const std::vector<uint8_t> &payload) {
    if(payload.size() != kResultPayloadSize) {
        THROW_IO_EXCEPTION_WITH_ERROR("Invalid recording session result response size", OB_ERROR_DEVICE_RESPONSE_WRONG_DATA_SIZE);
    }
    if(payload[0] > 1) {
        THROW_INVALID_DATA_EXCEPTION("Invalid recording session result file-exists value");
    }
    const auto recordingStatus = readUint32LE(payload.data() + sizeof(uint8_t) + sizeof(uint64_t));
    if(recordingStatus > static_cast<uint32_t>(OB_RECORDING_NOT_FOUND)) {
        THROW_INVALID_DATA_EXCEPTION("Invalid recording session result status");
    }

    OBRecordingSessionResult result{};
    result.fileExists        = payload[0] != 0;
    result.recordingDuration = readUint64LE(payload.data() + 1);
    result.recordingStatus   = static_cast<OBRecordingSessionStatus>(recordingStatus);
    result.reserved          = payload[sizeof(uint8_t) + sizeof(uint64_t) + sizeof(uint32_t)];
    return result;
}

}  // namespace libobsensor

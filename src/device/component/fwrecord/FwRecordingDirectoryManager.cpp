// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FwRecordingDirectoryManager.hpp"

#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "property/InternalProperty.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

namespace libobsensor {
namespace {

constexpr size_t kDirectoryMaxLength        = 480;
constexpr size_t kDirectorySegmentMaxLength = 255;
constexpr size_t kWireHeaderSize            = sizeof(uint16_t);

uint16_t readUint16LE(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

}  // namespace

FwRecordingDirectoryManager::FwRecordingDirectoryManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor)
    : DeviceComponentBase(owner),
      vendorPropertyAccessor_(std::move(vendorPropertyAccessor)) {
    if(!vendorPropertyAccessor_) {
        THROW_INVALID_PARAM_EXCEPTION("Firmware recording directory vendor property accessor is null");
    }
}

void FwRecordingDirectoryManager::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) {
    if(propertyId != OB_STRUCT_FW_RECORDING_DIRECTORY) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported firmware recording directory property id");
    }

    validateDirectory(data);

    auto propertyServer = getOwner()->getPropertyServer();
    if(propertyServer->getPropertyValueT<bool>(OB_PROP_FW_RECORDING_STATE_BOOL, PROP_ACCESS_INTERNAL)) {
        THROW_INVALID_PARAM_EXCEPTION("Cannot set firmware recording directory while firmware recording is active");
    }

    vendorPropertyAccessor_->setStructureData(propertyId, buildWirePayload(data));
}

std::vector<uint8_t> FwRecordingDirectoryManager::getStructureData(uint32_t propertyId) {
    if(propertyId != OB_STRUCT_FW_RECORDING_DIRECTORY) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported firmware recording directory property id");
    }

    try {
        return parseWirePayload(vendorPropertyAccessor_->getStructureData(propertyId));
    }
    catch(const libobsensor_exception &error) {
        // Some EGO firmware versions advertise this property but answer its
        // Get request with no payload or a malformed directory payload. Treat
        // only those optional-response errors as an unavailable directory so
        // callers can continue; transport and other protocol errors still
        // reach the application.
        const auto message = std::string(error.what());
        const bool emptyStructureData = message.find("get structure data return data size invalid") != std::string::npos;
        const bool malformedDirectoryData = error.getStatus() == OB_ERROR_INVALID_DATA
                                            && message.find("Firmware recording directory response") != std::string::npos;
        if(emptyStructureData || malformedDirectoryData) {
            LOG_WARN("Firmware recording directory is unavailable: property 1098 response is invalid ({})", message);
            return {};
        }
        throw;
    }
}

void FwRecordingDirectoryManager::validateDirectory(const std::vector<uint8_t> &directory) {
    if(directory.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: path must not be empty");
    }
    if(directory.size() > kDirectoryMaxLength) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: path exceeds 480 bytes");
    }
    if(!isValidUtf8(directory)) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: path must be valid UTF-8");
    }
    for(size_t index = 0; index < directory.size(); ++index) {
        const auto value = directory[index];
        if(value == 0 || value < 0x20 || value == 0x7F) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: control character is not allowed");
        }
        if(value == '\\') {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: backslash is not allowed");
        }
        // C1 control characters U+0080 through U+009F are encoded in UTF-8 as C2 80 through C2 9F.
        if(value == 0xC2 && index + 1 < directory.size() && directory[index + 1] >= 0x80 && directory[index + 1] <= 0x9F) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: control character is not allowed");
        }
    }

    size_t segmentBegin = directory.front() == '/' ? 1 : 0;
    while(segmentBegin < directory.size()) {
        const auto separator = std::find(directory.begin() + segmentBegin, directory.end(), static_cast<uint8_t>('/'));
        const auto segmentEnd = static_cast<size_t>(std::distance(directory.begin(), separator));
        const auto segmentLength = segmentEnd - segmentBegin;

        if(segmentLength == 0) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: empty path segment is not allowed");
        }
        if(segmentLength > kDirectorySegmentMaxLength) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: path segment exceeds 255 bytes");
        }

        const std::string segment(reinterpret_cast<const char *>(directory.data() + segmentBegin), segmentLength);
        if(segment == "." || segment == "..") {
            THROW_INVALID_PARAM_EXCEPTION("Invalid firmware recording directory: '.' and '..' are not allowed");
        }

        if(separator == directory.end()) {
            break;
        }
        segmentBegin = segmentEnd + 1;
    }
}

bool FwRecordingDirectoryManager::isValidUtf8(const std::vector<uint8_t> &data) {
    for(size_t index = 0; index < data.size();) {
        const auto first = data[index];
        if(first <= 0x7F) {
            ++index;
            continue;
        }

        size_t   sequenceLength = 0;
        uint32_t codePoint      = 0;
        uint32_t minimumValue   = 0;
        if((first & 0xE0) == 0xC0) {
            sequenceLength = 2;
            codePoint      = first & 0x1F;
            minimumValue   = 0x80;
        }
        else if((first & 0xF0) == 0xE0) {
            sequenceLength = 3;
            codePoint      = first & 0x0F;
            minimumValue   = 0x800;
        }
        else if((first & 0xF8) == 0xF0) {
            sequenceLength = 4;
            codePoint      = first & 0x07;
            minimumValue   = 0x10000;
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

std::vector<uint8_t> FwRecordingDirectoryManager::buildWirePayload(const std::vector<uint8_t> &directory) {
    const auto pathLength = static_cast<uint16_t>(directory.size());
    std::vector<uint8_t> payload(kWireHeaderSize + directory.size());
    payload[0] = static_cast<uint8_t>(pathLength & 0xFF);
    payload[1] = static_cast<uint8_t>((pathLength >> 8) & 0xFF);
    std::memcpy(payload.data() + kWireHeaderSize, directory.data(), directory.size());
    return payload;
}

std::vector<uint8_t> FwRecordingDirectoryManager::parseWirePayload(const std::vector<uint8_t> &payload) {
    if(payload.size() < kWireHeaderSize) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording directory response is too short");
    }

    const auto pathLength = readUint16LE(payload.data());
    if(pathLength == 0 || pathLength > kDirectoryMaxLength) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording directory response has an invalid path length");
    }
    if(payload.size() < kWireHeaderSize + pathLength) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording directory response is incomplete");
    }

    std::vector<uint8_t> directory(payload.begin() + kWireHeaderSize, payload.begin() + kWireHeaderSize + pathLength);
    validateDirectory(directory);
    return directory;
}

}  // namespace libobsensor

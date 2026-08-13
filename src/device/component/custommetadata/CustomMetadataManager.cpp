// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "CustomMetadataManager.hpp"

#include "IDevice.hpp"
#include "exception/ObException.hpp"
#include "protocol/Protocol.hpp"
#include "utils/Utils.hpp"

#include <json/json.h>

#include <cstring>
#include <fstream>
#include <utility>

namespace libobsensor {
namespace {

constexpr size_t kMaxSerializedJsonSize = 1000;
constexpr size_t kMaxProtocolPacketSize  = 1024;
constexpr size_t kSetStructureDataHeaderSize = sizeof(protocol::SetStructureDataReq) - 1;

#pragma pack(push, 1)
struct OBCustomMetadataJson {
    uint16_t jsonLength;  // Little-endian length of jsonData, excluding alignment padding.
    uint8_t  jsonData[1];
};
#pragma pack(pop)

static_assert(sizeof(OBCustomMetadataJson) == sizeof(uint16_t) + 1, "Custom metadata wire header must be packed");
static_assert(kSetStructureDataHeaderSize + sizeof(uint16_t) + kMaxSerializedJsonSize <= kMaxProtocolPacketSize,
              "Custom metadata request exceeds the protocol packet limit");

}  // namespace

CustomMetadataManager::CustomMetadataManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor)
    : DeviceComponentBase(owner), vendorPropertyAccessor_(std::move(vendorPropertyAccessor)) {
    if(!vendorPropertyAccessor_) {
        THROW_INVALID_PARAM_EXCEPTION("Custom metadata vendor property accessor is null");
    }
}

void CustomMetadataManager::setFromJsonFile(const std::string &jsonFilePath) {
    const auto serializedJson = loadAndSerializeJsonFile(jsonFilePath);
    getOwner()->getPropertyServer()->setStructureData(OB_STRUCT_CUSTOM_METADATA_JSON, serializedJson, PROP_ACCESS_USER);
}

void CustomMetadataManager::setStructureData(uint32_t propertyId, const std::vector<uint8_t> &serializedJson) {
    if(propertyId != OB_STRUCT_CUSTOM_METADATA_JSON) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported custom metadata property id");
    }
    vendorPropertyAccessor_->setStructureData(propertyId, buildWirePayload(serializedJson));
}

std::vector<uint8_t> CustomMetadataManager::getStructureData(uint32_t propertyId) {
    if(propertyId != OB_STRUCT_CUSTOM_METADATA_JSON) {
        THROW_INVALID_PARAM_EXCEPTION("Unsupported custom metadata property id");
    }
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("Reading custom metadata is not supported");
}

std::vector<uint8_t> CustomMetadataManager::loadAndSerializeJsonFile(const std::string &jsonFilePath) {
    if(jsonFilePath.empty()) {
        THROW_INVALID_PARAM_EXCEPTION("Custom metadata JSON file path is empty");
    }

    std::ifstream input(jsonFilePath, std::ios::binary);
    if(!input.is_open()) {
        THROW_IO_EXCEPTION(utils::string::to_string() << "Failed to open custom metadata JSON file: " << jsonFilePath);
    }

    Json::CharReaderBuilder readerBuilder;
    readerBuilder["collectComments"] = false;
    readerBuilder["allowComments"]   = false;
    readerBuilder["strictRoot"]      = true;

    Json::Value root;
    std::string parseErrors;
    if(!Json::parseFromStream(readerBuilder, input, &root, &parseErrors)) {
        THROW_INVALID_PARAM_EXCEPTION(utils::string::to_string() << "Invalid custom metadata JSON file: " << parseErrors);
    }
    if(!root.isObject()) {
        THROW_INVALID_PARAM_EXCEPTION("Custom metadata JSON root must be an object");
    }

    Json::StreamWriterBuilder writerBuilder;
    writerBuilder["indentation"]  = "";
    writerBuilder["commentStyle"] = "None";
    writerBuilder["emitUTF8"]     = true;
    auto serializedJson = Json::writeString(writerBuilder, root);
    if(!serializedJson.empty() && serializedJson.back() == '\n') {
        serializedJson.pop_back();
    }
    if(serializedJson.empty() || serializedJson.size() > kMaxSerializedJsonSize) {
        THROW_INVALID_PARAM_EXCEPTION("Custom metadata JSON exceeds 1000 bytes after serialization");
    }

    return std::vector<uint8_t>(serializedJson.begin(), serializedJson.end());
}

std::vector<uint8_t> CustomMetadataManager::buildWirePayload(const std::vector<uint8_t> &serializedJson) {
    if(serializedJson.empty() || serializedJson.size() > kMaxSerializedJsonSize) {
        THROW_INVALID_PARAM_EXCEPTION("Custom metadata JSON exceeds 1000 bytes after serialization");
    }

    const auto jsonLength = static_cast<uint16_t>(serializedJson.size());
    std::vector<uint8_t> payload(sizeof(OBCustomMetadataJson) - 1 + serializedJson.size());
    payload[0] = static_cast<uint8_t>(jsonLength & 0xFF);
    payload[1] = static_cast<uint8_t>((jsonLength >> 8) & 0xFF);
    std::memcpy(payload.data() + sizeof(OBCustomMetadataJson) - 1, serializedJson.data(), serializedJson.size());

    // SET_STRUCTURE_DATA encodes its size in halfwords. Keep the actual
    // transfer length aligned with that field while jsonLength preserves the
    // true JSON byte count for the firmware parser.
    if(payload.size() % 2 != 0) {
        payload.push_back(0);
    }
    return payload;
}

}  // namespace libobsensor

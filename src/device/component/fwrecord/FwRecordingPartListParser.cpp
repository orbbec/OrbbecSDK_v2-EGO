// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "FwRecordingPartListParser.hpp"

#include "IDevice.hpp"
#include "exception/ObException.hpp"

#include <cstring>

namespace libobsensor {
namespace {

std::vector<OBFwRecordingPartEntry> parseFwRecordingPartList(const std::vector<uint8_t> &payload) {
    if(payload.size() < sizeof(OBFwRecordingPartListHeader)) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording part list response is too short");
    }

    OBFwRecordingPartListHeader header{};
    std::memcpy(&header, payload.data(), sizeof(header));
    if(header.protocol_version != OB_FW_RECORDING_PART_LIST_VERSION) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording part list has an unsupported protocol version");
    }
    if(header.entry_size != OB_FW_RECORDING_PART_NAME_SIZE) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording part list has an invalid entry size");
    }

    const uint64_t expectedSize = sizeof(OBFwRecordingPartListHeader)
                                  + static_cast<uint64_t>(header.part_count) * header.entry_size;
    if(expectedSize != payload.size()) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording part list has an invalid payload size");
    }

    std::vector<OBFwRecordingPartEntry> entries(header.part_count);
    if(!entries.empty()) {
        std::memcpy(entries.data(), payload.data() + sizeof(OBFwRecordingPartListHeader), entries.size() * sizeof(entries.front()));
    }
    return entries;
}

}  // namespace

std::vector<OBFwRecordingPartEntry> getFwRecordingPartList(IDevice *device, PropertyAccessType accessType) {
    if(device == nullptr) {
        THROW_INVALID_PARAM_EXCEPTION("Firmware recording part list device is null");
    }

    std::vector<uint8_t> payload;
    bool                 transferDone = false;
    auto propertyServer = device->getPropertyServer();
    propertyServer->getRawData(
        OB_RAW_DATA_FW_RECORDING_PART_LIST,
        [&payload, &transferDone](OBDataTranState state, OBDataChunk *dataChunk) {
            if(state == DATA_TRAN_STAT_TRANSFERRING && dataChunk && dataChunk->data && dataChunk->size > 0) {
                payload.insert(payload.end(), dataChunk->data, dataChunk->data + dataChunk->size);
            }
            else if(state == DATA_TRAN_STAT_DONE) {
                transferDone = true;
            }
        },
        accessType);

    if(!transferDone) {
        THROW_INVALID_DATA_EXCEPTION("Firmware recording part list transfer did not complete");
    }
    return parseFwRecordingPartList(payload);
}

}  // namespace libobsensor

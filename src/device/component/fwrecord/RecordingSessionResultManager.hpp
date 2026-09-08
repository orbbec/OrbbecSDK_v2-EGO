// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "component/DeviceComponentBase.hpp"
#include "IProperty.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace libobsensor {

class RecordingSessionResultManager : public DeviceComponentBase {
public:
    RecordingSessionResultManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor);

    OBRecordingSessionResult getRecordingSessionResult(const std::string &storagePath);

private:
    static void                 validateStoragePath(const std::vector<uint8_t> &storagePath);
    static bool                 isValidUtf8(const std::vector<uint8_t> &data);
    static std::vector<uint8_t> buildPathPayload(const std::vector<uint8_t> &storagePath);
    static OBRecordingSessionResult parseResultPayload(const std::vector<uint8_t> &payload);

private:
    std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor_;
    std::mutex                               queryMutex_;
};

}  // namespace libobsensor

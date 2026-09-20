// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "component/DeviceComponentBase.hpp"
#include "IProperty.hpp"

#include <memory>
#include <vector>

namespace libobsensor {

class FwRecordingDirectoryManager : public DeviceComponentBase, public IStructureDataAccessor {
public:
    FwRecordingDirectoryManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor);

    IDevice *getOwner() const override {
        return DeviceComponentBase::getOwner();
    }

    void                 setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    std::vector<uint8_t> getStructureData(uint32_t propertyId) override;

private:
    static void                 validateDirectory(const std::vector<uint8_t> &directory);
    static void                 validateFileName(const std::vector<uint8_t> &fileName);
    static bool                 isValidUtf8(const std::vector<uint8_t> &data);
    static std::vector<uint8_t> buildWirePayload(const std::vector<uint8_t> &directory);
    static std::vector<uint8_t> parseWirePayload(const std::vector<uint8_t> &payload);

private:
    std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor_;
};

}  // namespace libobsensor

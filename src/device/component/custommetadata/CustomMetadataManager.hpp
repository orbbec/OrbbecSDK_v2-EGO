// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "ICustomMetadataManager.hpp"
#include "IProperty.hpp"
#include "component/DeviceComponentBase.hpp"

#include <memory>
#include <vector>

namespace libobsensor {

class CustomMetadataManager : public ICustomMetadataManager, public DeviceComponentBase, public IStructureDataAccessor {
public:
    CustomMetadataManager(IDevice *owner, std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor);

    IDevice *getOwner() const override {
        return DeviceComponentBase::getOwner();
    }

    void setFromJsonFile(const std::string &jsonFilePath) override;
    void setStructureData(uint32_t propertyId, const std::vector<uint8_t> &data) override;
    std::vector<uint8_t> getStructureData(uint32_t propertyId) override;

private:
    static std::vector<uint8_t> loadAndSerializeJsonFile(const std::string &jsonFilePath);
    static std::vector<uint8_t> buildWirePayload(const std::vector<uint8_t> &serializedJson);

private:
    std::shared_ptr<IStructureDataAccessor> vendorPropertyAccessor_;
};

}  // namespace libobsensor

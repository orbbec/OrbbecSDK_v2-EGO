// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IDeviceComponent.hpp"

#include <string>

namespace libobsensor {

class ICustomMetadataManager : virtual public IDeviceComponent {
public:
    ~ICustomMetadataManager() override = default;

    virtual void setFromJsonFile(const std::string &jsonFilePath) = 0;
};

}  // namespace libobsensor

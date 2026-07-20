// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "component/DeviceComponentBase.hpp"
#include "IDeviceComponent.hpp"
#include "libobsensor/h/ObTypes.h"

#include <cstdint>

namespace libobsensor {

class IFwRecordProfileManager : virtual public IDeviceComponent {
public:
    virtual ~IFwRecordProfileManager() = default;

    virtual void getProfileList(ob_fw_record_stream_profile *profiles, uint32_t *profileCount) = 0;
    virtual void setProfileList(const ob_fw_record_stream_profile *profiles, uint32_t profileCount) = 0;
};

class FwRecordProfileManager : public IFwRecordProfileManager, public DeviceComponentBase {
public:
    explicit FwRecordProfileManager(IDevice *owner);

    IDevice *getOwner() const override {
        return DeviceComponentBase::getOwner();
    }

    void getProfileList(ob_fw_record_stream_profile *profiles, uint32_t *profileCount) override;
    void setProfileList(const ob_fw_record_stream_profile *profiles, uint32_t profileCount) override;
};

}  // namespace libobsensor

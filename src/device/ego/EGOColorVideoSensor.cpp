// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOColorVideoSensor.hpp"

#include "usb/uvc/UvcDevicePort.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "component/property/InternalProperty.hpp"

namespace libobsensor {

EGOColorVideoSensor::EGOColorVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend)
    : VideoSensor(owner, sensorType, backend) {}

void EGOColorVideoSensor::trySendStopStreamVendorCmd() {
    auto uvcPort = std::dynamic_pointer_cast<UvcDevicePort>(backend_);
    if(!uvcPort) {
        return;
    }
    // EGO firmware assigns bUnitID=2 to the Orbbec vendor XU; update before creating the accessor
    // so that Linux/libuvc (which routes by unit number) targets the correct unit.
    uvcPort->updateXuUnit(OB_EGO_XU_UNIT);
    auto            accessor = std::make_shared<VendorPropertyAccessor>(getOwner(), backend_);
    OBPropertyValue value;
    value.intValue = 1;
    TRY_EXECUTE({ accessor->setPropertyValue(OB_PROP_STOP_COLOR_STREAM_BOOL, value); })
}

}  // namespace libobsensor

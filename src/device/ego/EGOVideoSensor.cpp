// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOVideoSensor.hpp"

#include "usb/uvc/UvcDevicePort.hpp"
#include "property/VendorPropertyAccessor.hpp"
#include "component/property/InternalProperty.hpp"
#include "exception/ObException.hpp"

namespace libobsensor {

EGOVideoSensor::EGOVideoSensor(IDevice *owner, OBSensorType sensorType, const std::shared_ptr<ISourcePort> &backend, uint32_t stopStreamPropertyId)
    : VideoSensor(owner, sensorType, backend), stopStreamPropertyId_(stopStreamPropertyId) {
    // The base VideoSensor constructor already called trySendStopStreamVendorCmd() to clear any stream
    // left over from a previous crash, but a virtual call during base construction does NOT reach a
    // derived override (the vtable still points to the base), so only the base (no-op for EGO) version
    // ran. Repeat it now that the object is fully constructed, so the configured stop command goes
    // out on this sensor's own interface XU.
    TRY_EXECUTE({ trySendStopStreamVendorCmd(); });
}

EGOVideoSensor::~EGOVideoSensor() noexcept {
    // ~VideoSensor() also calls stop() -> trySendStopStreamVendorCmd(), but by the time the base
    // destructor runs the vtable has reverted to the base class, so the per-eye override would not
    // fire. Call stop() here from the most-derived destructor so the override runs. stop() is
    // idempotent (it early-returns once the stream is stopped), so ~VideoSensor()'s later stop() is a
    // no-op and the vendor command is not sent twice.
    TRY_EXECUTE({ stop(); })
}

void EGOVideoSensor::trySendStopStreamVendorCmd() {
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
    TRY_EXECUTE({ accessor->setPropertyValue(stopStreamPropertyId_, value); })
}

}  // namespace libobsensor

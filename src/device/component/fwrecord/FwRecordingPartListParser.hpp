// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "IProperty.hpp"
#include "libobsensor/h/ObTypes.h"

#include <vector>

namespace libobsensor {

class IDevice;

std::vector<OBFwRecordingPartEntry> getFwRecordingPartList(IDevice *device, PropertyAccessType accessType);

}  // namespace libobsensor

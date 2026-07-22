// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace libobsensor {

enum OBHidReportId : uint8_t {
    OB_HID_REPORT_ID_IMU                     = 1,
    OB_HID_REPORT_ID_DIRECTION_KEY           = 2,
    OB_HID_REPORT_ID_BATTERY_INFOMATION_DATA = 3,
    OB_HID_REPORT_ID_DEVICE_STATE           = 4,
};

#pragma pack(push, 1)
typedef struct {
    uint8_t  reportId;
    uint8_t  sampleRate;
    uint8_t  groupLen;
    uint8_t  groupCount;
    uint32_t reserved;
} OBHidHeader;
#pragma pack(pop)

}  // namespace libobsensor

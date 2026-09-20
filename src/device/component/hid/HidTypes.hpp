// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace libobsensor {

enum OBHidReportId : uint8_t {
    OB_HID_REPORT_ID_IMU                     = 1,
    OB_HID_REPORT_ID_DIRECTION_KEY           = 2,
    OB_HID_REPORT_ID_BATTERY_INFOMATION_DATA = 3,
    OB_HID_REPORT_ID_DEVICE_STATE            = 4,
    OB_HID_REPORT_ID_MAG_ENCODER             = 5,
    OB_HID_REPORT_ID_MAGNETOMETER            = 6,
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

// Raw magnetic angle encoder (MT6826S) data group carried by a reportId=5 HID packet.
// The packet layout is OBHidHeader followed by groupCount groups of groupLen=24 bytes.
// The group is demultiplexed only by OBHidHeader.reportId; the first field is groupId (the
// index of this group inside the pack), matching the OBImuOriginData / OBMagnetometerData
// convention. Padded to 24 bytes so the group size matches OBMagnetometerData.
#pragma pack(push, 1)
typedef struct {
    uint8_t  groupId;    // group index inside the pack
    uint8_t  version;    // report layout version (currently 1)
    uint16_t angle;  // raw angle
    uint8_t  status;     // bit0 overspeed, bit1 weak magnetic field, bit2 undervoltage
    uint8_t  reserved[11];
    uint64_t timestamp;  // device timestamp, microseconds (CLOCK_MONOTONIC)
} Mt6826sAngleReport;
#pragma pack(pop)

static_assert(sizeof(Mt6826sAngleReport) == 24, "Mt6826sAngleReport must be 24 bytes");

}  // namespace libobsensor

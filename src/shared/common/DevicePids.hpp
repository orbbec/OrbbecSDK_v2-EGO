// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include "DeviceSeriesInfo.hpp"
namespace libobsensor {
const std::vector<uint16_t> BootDevPids = {
    0x0501,  // bootloader
    0x0300,  // openni bootloader
    0x0301,  // openni bootloader
    0x0400,  // openni Mx400/Mx600 bootloader
};

const std::vector<uint16_t> Astra2DevPids = {
    0x0660,  // astra2
};

const uint16_t OB_DEVICE_G336_PID   = 0x0803;
const uint16_t OB_DEVICE_G336L_PID  = 0x0807;
const uint16_t OB_DEVICE_G335LE_PID = 0x080E;

const std::vector<uint16_t> Gemini2DevPids = {
    0x0670,  // Gemini2
    0x0673,  // Gemini2L
    // 0x0671,  // Gemini2XL // remove g2xl support temporarily as it is currently not fully supported
    0x0808,  // Gemini215
    0x0809,  // Gemini210
};

const std::vector<uint16_t> FemtoMegaDevPids = {
    0x0669,  // Femto Mega
    0x06c0,  // Femto Mega i
};
#define OB_FEMTO_MEGA_PID 0x0669
#define IS_OB_FEMTO_MEGA_I_PID(pid) (((pid) == 0x06c0))

const std::vector<uint16_t> FemtoBoltDevPids = {
    0x066B,  // Femto Bolt
};

// OpenNI Devices
const uint16_t OB_DEVICE_DCW_PID            = 0x0659;
const uint16_t OB_DEVICE_DW_PID             = 0x065a;
const uint16_t OB_DEVICE_GEMINI_E_PID       = 0x065c;
const uint16_t OB_DEVICE_GEMINI_E_LITE_PID  = 0x065d;
const uint16_t OB_DEVICE_DABAI_MAX_PID      = 0x069a;
const uint16_t OB_DEVICE_DABAI_DC1_PID      = 0x0657;
const uint16_t OB_DEVICE_DABAI_DW2_PID      = 0x069f;
const uint16_t OB_DEVICE_GEMINI_EW_LITE_PID = 0x06a7;
const uint16_t OB_DEVICE_DABAI_DCW2_PID     = 0x06a0;
const uint16_t OB_DEVICE_GEMINI_EW_PID      = 0x06a6;
const uint16_t OB_DEVICE_MAX_PRO_PID        = 0x069e;
const uint16_t OB_DEVICE_GEMINI_UW_PID      = 0x06aa;
const uint16_t OB_DEVICE_MINI_PRO_PID       = 0x065b;
const uint16_t OB_DEVICE_MINI_S_PRO_PID     = 0x065e;
const uint16_t OB_DEVICE_ASTRA_PRO2_PID     = 0x069d;

const std::vector<uint16_t> OpenNIDevPids = {
    0x060e,  // Dabai
    0x0614,  // Gemini
    0x0657,  // Dabai DC1
    0x0659,  // Dabai DCW
    0x065a,  // Dabai DW
    0x065b,  // Astra Mini Pro
    0x065c,  // Gemini E
    0x065d,  // Gemini E Lite
    0x065e,  // Astra mini s pro
    0x069a,  // Dabai Max
    0x069f,  // Dabai DW2
    0x06a7,  // Gemini EW Lite
    0x06a0,  // Dabai DCW2
    0x06a6,  // Gemini EW
    0x069e,  // Dabai Max Pro
    0x06aa,  // Gemini UW
    0x069d,  // Astra Pro2
};

const std::vector<uint16_t> OpenniAstraPids = {
    0x0407,  // Astra mini S
    0x065a,  // Dabai DW
    0x065b,  // Astra Mini Pro
    0x065d,  // Gemini E Lite
    0x065e,  // Astra mini s pro
    0x069a,  // Dabai Max
    0x069f,  // Dabai DW2
    0x069d,  // Astra Pro2
    0x06a7,  // Gemini EW Lite
};

const std::vector<uint16_t> OpenniRgbPids = {

    0x050b,  // deeyea
    0x050e,  // Dabai
    0x0511,  // gemini
    0x0555,  // Dabai plus
    0x0557,  // Dabai DC1
    0x0559,  // Dabai DCW
    0x055c,  // Gemini E
    0x0561,  // Dabai DCW2
    0x05a6,  // Gemini EW
    0x0560,  // Dabai Max Pro
    0x05aa   // Gemini UW
};

const std::vector<uint16_t> OpenNIDualPids = {
    0x060e,  // Dabai
    0x0657,  // Dabai DC1
    0x0659,  // Dabai DCW
    0x065a,  // Dabai DW
    0x065c,  // Gemini E
    0x065d,  // Gemini E Lite
    0x069a,  // Dabai Max
    0x069f,  // Dabai DW2
    0x06a7,  // Gemini EW Lite
    0x06a0,  // Dabai DCW2
    0x06a6,  // Gemini EW
    0x069e,  // Dabai Max Pro
    0x06aa,  // Gemini UW
};

// monocular
const std::vector<uint16_t> OpenniMonocularPids = {
    0x0407,  // Astra mini S
    0x065b,  // Astra Mini Pro
    0x065e,  // Astra mini s pro
    0x069d,  // Astra Pro2
};

const std::vector<uint16_t> OpenniDW2Pids = {
    0x069f,  // Dabai DW2
    0x06a7,  // Gemini EW Lite
    0x06a0,  // Dabai DCW2
    0x06a6,  // Gemini EW
};

const std::vector<uint16_t> OpenniMaxPids = {
    0x069a,  // Dabai Max
    0x069e,  // Dabai Max Pro
    0x06aa,  // Gemini UW
};

const std::vector<uint16_t> G305DevPids = {
    0x0840,  // Gemini 305
    0x0841,  // Gmeini 305
    0x0842,  // Gemini 305g
    0x0843,  // Gemini 301g
    0x0845   // Gemini 309g
};

// PN2608_EGO: head-mounted dual-RGB + IMU + audio device (USB-UVC/UAC and WiFi-RTSP paths)
#define OB_DEVICE_EGO_PID 0x1201
const std::vector<uint16_t> EGODevPids = {
    0x1201,  // PN2608_EGO
};

// ===================== [EGO-SIM] TEMPORARY: remove once real EGO hardware is available =====================
// Simulate an EGO device using a USB Femto Mega so the EGODevice / EGOAlgParamManager path can be
// exercised without EGO hardware. When enabled, a USB Femto Mega (0x0669) is routed through EGODevice:
//   depth (UVC inf 2) -> COLOR_LEFT, color (UVC inf 0) -> COLOR_RIGHT, IMU (HID) -> accel/gyro, audio not paired.
// To restore normal behavior, comment out this define.
//
// REMOVAL CHECKLIST -- grep the tag "[EGO-SIM]" across the repo. All simulation code is either gated by
// the EGO_SIMULATE_WITH_FEMTO_MEGA macro below or tagged "[EGO-SIM]". Locations:
//   - src/device/DevicePids.hpp                 : this block (macro + EGO_SIM_* defines)
//   - src/device/ego/EGODevice.cpp              : interface-index swap, OB_SENSOR_DEPTH/COLOR aliases,
//                                                 and the initSensorStreamProfile() binding condition
//   - src/device/ego/EGODeviceInfo.cpp          : claim Femto Mega PID + synthetic mic UAC port (Windows) +
//                                                 real mic UAC port pairing (libusb / Linux)
//   - src/device/femtomega/FemtoMegaDeviceInfo.cpp : drop Femto Mega PID + <algorithm> include
//   - src/platform/usb/uac/WasapiUacDevicePort.cpp : friendly-name fallback match (NOT macro-gated)
//   - src/shared/common/DeviceSeriesInfo.cpp    : whitelist external mic VID 0x1BCF for libusb enumeration
//                                                 (NOT macro-gated; shared layer cannot include this header)
#define EGO_SIMULATE_WITH_FEMTO_MEGA
// PID of the USB Femto Mega used as the EGO stand-in (only the plain Femto Mega, not Femto Mega i).
#define EGO_SIM_FEMTO_MEGA_PID 0x0669
// The Femto Mega has no microphone, so an external USB microphone (XWF-1080P) stands in for the EGO
// audio. On Windows the WASAPI backend matches the capture endpoint by VID/PID, so a synthetic UAC port
// carrying these IDs is paired onto the simulated EGO device.
#define EGO_SIM_MIC_VID 0x1BCF
#define EGO_SIM_MIC_PID 0x2281
// Windows audio endpoints often expose a SWD\MMDEVAPI instance id with no USB VID/PID, so the WASAPI
// backend also matches by endpoint friendly-name substring. This is the external microphone's name.
#define EGO_SIM_MIC_NAME "XWF-1080P"

#define LIDAR_PID_ME450_OLD 0x5555
#define LIDAR_PID_ME450 0x1302
#define LIDAR_PID_MS600 0x1300
#define LIDAR_PID_SL450 0x1301
const std::vector<uint16_t> LiDARDevPids = {
    LIDAR_PID_MS600,      // Single-line LiDAR MS600
    LIDAR_PID_SL450,      // Single-line LiDAR SL450
    LIDAR_PID_ME450,      // Multi-lines LiDAR ME450
    LIDAR_PID_ME450_OLD,  // Multi-lines LiDAR ME450 old pid
};

const std::map<std::string, uint32_t> LiDARDeviceNameMap = {
    { "MS600", LIDAR_PID_MS600 },  // Single-line LiDAR MS600
    { "SL450", LIDAR_PID_SL450 },  // Single-line LiDAR international Version of MS600
    { "ME450", LIDAR_PID_ME450 },  // Multi-lines LiDAR ME450
};

#define IS_OB_LIDAR_SINGLE_LINE(pid) (((pid) == LIDAR_PID_MS600) || ((pid) == LIDAR_PID_SL450))
#define IS_OB_LIDAR_MULTI_LINE(pid) (((pid) == LIDAR_PID_ME450) || ((pid) == LIDAR_PID_ME450_OLD))
}  // namespace libobsensor
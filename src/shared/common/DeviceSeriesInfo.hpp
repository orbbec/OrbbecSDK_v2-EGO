#pragma once

#include "libobsensor/h/ObTypes.h"
#include "CommonFields.hpp"
#include "utils/Utils.hpp"
#include "xml/XmlReader.hpp"

#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <string>
#include <initializer_list>

namespace libobsensor {

// EgoPro firmware upgrade board (RockUSB download gadget).
#define EGOPRO_BOADER_VID 0x2207
#define EGOPRO_BOADER_PID 0x350E
#define UMI_FINGER_BOADER_PID 0x110F

extern std::vector<DeviceIdentifier> G330DevPids;
extern std::vector<DeviceIdentifier> G330LDevPids;
extern std::vector<DeviceIdentifier> DaBaiADevPids;
extern std::vector<DeviceIdentifier> G335LeDevPids;
extern std::vector<DeviceIdentifier> G435LeDevPids;
extern std::vector<DeviceIdentifier> EGODevPids;
extern std::vector<DeviceIdentifier> EGOProDevPids;
extern std::vector<DeviceIdentifier> EGODepthDevPids;
extern std::vector<DeviceIdentifier> WristDevPids;

extern std::vector<DeviceInfoEntry> G330DeviceInfoList;
extern std::vector<DeviceInfoEntry> G435LeDeviceInfoList;
extern std::vector<DeviceInfoEntry> EGODeviceInfoList;
extern std::vector<DeviceInfoEntry> WristDeviceInfoList;

// List of allowed vendor IDs
extern std::unordered_set<uint16_t>              supportedUsbVids;
extern std::unordered_map<std::string, uint16_t> manufacturerVidMap;

bool isDeviceInContainer(const std::vector<DeviceIdentifier> &deviceContainer, const uint32_t &vid, const uint32_t &pid);
bool isDeviceInOrbbecSeries(const std::vector<uint16_t> &deviceContainer, const uint32_t &vid, const uint32_t &pid);

/**
 * @brief Check if the device is an EGOPro series device (four-RGB shared UVC endpoint).
 *
 * Membership is driven by the EGOProDeviceList category in DeviceInfoConfig.xml, so a new
 * EGOPro-based ODM only needs an XML entry instead of a hardcoded PID branch in code.
 *
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @return true if the device belongs to the EGOPro series
 */
bool isEgoProDevice(uint32_t vid, uint32_t pid);

/**
 * @brief Check if the device is an EGODepth series device.
 *
 * EGODepth evolves from EgoPro: 3 Color cameras multiplexed on one shared UVC endpoint, 2 IR cameras
 * on a second shared endpoint, plus an exclusive Depth endpoint and an exclusive single Color endpoint.
 * Membership is driven by the EGODepthDeviceList category in DeviceInfoConfig.xml, so a new EGODepth-based
 * ODM only needs an XML entry instead of a hardcoded PID branch in code.
 *
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @return true if the device belongs to the EGODepth series
 */
bool isEgoDepthDevice(uint32_t vid, uint32_t pid);

/**
 * @brief Check if the device is supported by the library, used for platform enumeration
 *
 * @param[in] vid Vendor ID
 * @param[in] pid Product ID
 * @return true if the device is supported
 */
bool isSupportedDevice(uint32_t vid, uint32_t pid);

/**
 * @brief Check if the bootloader network device is supported by the library, used for platform enumeration
 *
 * @param[in] manufacturer Manufacturer
 * @param[in] pid Product ID
 * @param[out] vid Vendor ID parsed from the manufacturer string
 * @return true if the device is supported
 */
bool isSupportedOemBootloaderNetworkDevice(const std::string &manufacturer, uint32_t pid, uint32_t &vid);

class DeviceSeriesInfoManager {
public:
    // Get singleton instance
    static std::shared_ptr<DeviceSeriesInfoManager> getInstance();

private:
    DeviceSeriesInfoManager();

    // Load additional device configuration from XML
    void loadXmlConfig(const std::string &configFileName, bool isExternalConfig);

private:
    static std::weak_ptr<DeviceSeriesInfoManager> instanceWeakPtr_;
    static std::mutex                             instanceMutex_;
};

}  // namespace libobsensor

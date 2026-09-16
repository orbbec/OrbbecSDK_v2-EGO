// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "libobsensor/h/Device.h"

#include "ImplTypes.hpp"
#include "exception/ObException.hpp"
#include "firmwareupdater/FirmwareUpdater.hpp"
#include "shared/utils/Utils.hpp"

#include "IDeviceManager.hpp"
#include "devicemanager/DeviceManager.hpp"
#include "license/IDeviceLicenseInfoManager.hpp"
#include "IProperty.hpp"
#include "IDevice.hpp"
#include "IDeviceMonitor.hpp"

#if defined(BUILD_NET_PAL)
#include "accesscontroller/GvcpCcpController.hpp"
#endif
#include "IAlgParamManager.hpp"
#include "IFrameTimestamp.hpp"
#include "component/fwrecord/FwRecordProfileManager.hpp"
#include "component/fwrecord/FwRecordingPartListParser.hpp"
#include "component/fwrecord/RecordingSessionResultManager.hpp"
#include "ICustomMetadataManager.hpp"
#include "component/fwlog/FirmwareLogManager.hpp"
#include "component/audio/IAudioPlayer.hpp"
#include "hid/EgoStateService.hpp"
#include "hid/BatteryInfoService.hpp"
#include "hid/DirectionKeyService.hpp"
#include "utils/PublicTypeHelper.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <set>
#include <vector>

namespace {

static_assert(sizeof(ob_fw_recording_mcap_topic) == 130, "MCAP topic protocol item must occupy 130 bytes");
static_assert(sizeof(ob_fw_recording_mcap_topic) % 2 == 0, "MCAP topic protocol item must have an even byte length");

constexpr size_t kDeviceUserNameBufferSize = 32;
constexpr size_t kDeviceModelTypeBufferSize = 32;
constexpr size_t kBluetoothPairingPayloadHeaderSize = 8;
constexpr int32_t kBluetoothClearPairingValue = 1;
constexpr int32_t kClearWifiConfigValue = 1;

uint16_t readLittleEndianUint16(const uint8_t *data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLittleEndianUint32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16)
           | (static_cast<uint32_t>(data[3]) << 24);
}

OBBluetoothPairingInfo parseBluetoothPairingInfo(const std::vector<uint8_t> &payload) {
    if(payload.size() < kBluetoothPairingPayloadHeaderSize) {
        THROW_IO_EXCEPTION_WITH_ERROR("Bluetooth pairing payload is shorter than the header", OB_ERROR_DEVICE_RESPONSE_WRONG_DATA_SIZE);
    }

    const auto pairingState = payload[0];
    const auto pairingCount = payload[1];
    const auto reserved = readLittleEndianUint16(payload.data() + 2);
    const auto pairingTimeS = readLittleEndianUint32(payload.data() + 4);

    if(pairingState != OB_BLUETOOTH_PAIRING_STATE_UNPAIRED && pairingState != OB_BLUETOOTH_PAIRING_STATE_VALID
       && pairingState != OB_BLUETOOTH_PAIRING_STATE_INVALID) {
        THROW_INVALID_DATA_EXCEPTION("Invalid Bluetooth pairing state");
    }
    if(reserved != 0) {
        THROW_INVALID_DATA_EXCEPTION("Bluetooth pairing payload reserved field is non-zero");
    }
    if(pairingCount > OB_BLUETOOTH_PAIRED_DEVICE_MAX_COUNT) {
        THROW_INVALID_DATA_EXCEPTION("Bluetooth pairing record count exceeds the maximum");
    }
    if(pairingState == OB_BLUETOOTH_PAIRING_STATE_UNPAIRED && (pairingCount != 0 || pairingTimeS != 0)) {
        THROW_INVALID_DATA_EXCEPTION("Unpaired device has Bluetooth pairing records or pairing time");
    }
    if(pairingState != OB_BLUETOOTH_PAIRING_STATE_UNPAIRED && pairingCount == 0) {
        THROW_INVALID_DATA_EXCEPTION("Paired device has no Bluetooth pairing records");
    }

    OBBluetoothPairingInfo result{};
    result.pairing_state = pairingState;
    result.paired_device_count = pairingCount;
    result.pairing_time_s = pairingTimeS;

    size_t offset = kBluetoothPairingPayloadHeaderSize;
    for(uint8_t index = 0; index < pairingCount; ++index) {
        if(payload.size() - offset < sizeof(uint16_t)) {
            THROW_IO_EXCEPTION_WITH_ERROR("Bluetooth pairing payload is missing a device name length", OB_ERROR_DEVICE_RESPONSE_WRONG_DATA_SIZE);
        }

        const auto nameLength = readLittleEndianUint16(payload.data() + offset);
        offset += sizeof(uint16_t);
        if(nameLength == 0 || nameLength > OB_BLUETOOTH_DEVICE_NAME_MAX_SIZE) {
            THROW_INVALID_DATA_EXCEPTION("Bluetooth pairing device name length is invalid");
        }
        if(payload.size() - offset < nameLength) {
            THROW_IO_EXCEPTION_WITH_ERROR("Bluetooth pairing payload device name is truncated", OB_ERROR_DEVICE_RESPONSE_WRONG_DATA_SIZE);
        }

        auto &record = result.paired_devices[index];
        record.name_length = nameLength;
        std::memcpy(record.device_name, payload.data() + offset, nameLength);
        offset += nameLength;
    }

    if(payload.size() - offset > 1) {
        THROW_INVALID_DATA_EXCEPTION("Bluetooth pairing payload has unexpected trailing data");
    }
    return result;
}

bool isValidMcapTopicType(uint16_t topicType) {
    switch(topicType) {
    case OB_FW_MCAP_TOPIC_COLOR:
    case OB_FW_MCAP_TOPIC_COLOR_LEFT:
    case OB_FW_MCAP_TOPIC_COLOR_RIGHT:
    case OB_FW_MCAP_TOPIC_COLOR_SIDE_LEFT:
    case OB_FW_MCAP_TOPIC_COLOR_SIDE_RIGHT:
    case OB_FW_MCAP_TOPIC_ACCEL:
    case OB_FW_MCAP_TOPIC_GYRO:
    case OB_FW_MCAP_TOPIC_AUDIO:
        return true;
    default:
        return false;
    }
}

void validateMcapTopicList(const ob_fw_recording_mcap_topic *topics, uint32_t topicCount) {
    if(topicCount > 0 && topics == nullptr) {
        THROW_INVALID_PARAM_EXCEPTION("MCAP topic list is null");
    }
    if(static_cast<uint64_t>(topicCount)
       > static_cast<uint64_t>((std::numeric_limits<size_t>::max)()) / sizeof(ob_fw_recording_mcap_topic)) {
        THROW_INVALID_PARAM_EXCEPTION("MCAP topic list is too large");
    }

    std::set<uint16_t> topicTypes;
    for(uint32_t i = 0; i < topicCount; ++i) {
        const auto &topic = topics[i];
        if(!isValidMcapTopicType(topic.topicType)) {
            THROW_INVALID_PARAM_EXCEPTION("MCAP topic has an invalid topic type");
        }
        if(std::memchr(topic.topic, '\0', sizeof(topic.topic)) == nullptr) {
            THROW_INVALID_PARAM_EXCEPTION("MCAP topic is not NUL-terminated");
        }
        if(topic.topic[0] == '\0') {
            THROW_INVALID_PARAM_EXCEPTION("MCAP topic is empty");
        }
        if(!topicTypes.insert(topic.topicType).second) {
            THROW_INVALID_PARAM_EXCEPTION("MCAP topic list has duplicate topic types");
        }
    }
}

std::vector<uint8_t> getMcapTopicListData(libobsensor::IDevice *device) {
    std::vector<uint8_t> data;
    auto                 propertyServer = device->getPropertyServer();
    propertyServer->getRawData(
        OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST,
        [&data](OBDataTranState state, OBDataChunk *dataChunk) {
            if(state == DATA_TRAN_STAT_TRANSFERRING && dataChunk && dataChunk->data && dataChunk->size > 0) {
                data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
            }
        },
        libobsensor::PROP_ACCESS_USER);
    return data;
}

}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

void ob_delete_device_list(ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    delete list;
}
HANDLE_EXCEPTIONS_NO_RETURN(list)

uint32_t ob_device_list_get_count(const ob_device_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    return static_cast<uint32_t>(list->list.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

const char *ob_device_list_get_device_name(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getFullName().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

int ob_device_list_get_device_pid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getPid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

int ob_device_list_get_device_vid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getVid();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list, index)

const char *ob_device_list_get_device_uid(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getUid().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_serial_number(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getDeviceSn().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_connection_type(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    return info->getConnectionType().c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_ip_address(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get device IP address: valid only for Ethernet devices.");
        return "0.0.0.0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->address.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_subnet_mask(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get subnet mask: valid only for Ethernet devices.");
        return "0.0.0.0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->mask.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_gateway(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get gateway: valid only for Ethernet devices.");
        return "0.0.0.0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->gateway.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_local_mac(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get local net iface mac address: valid only for Ethernet devices.");
        return "0:0:0:0:0:0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->localMac.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_local_ip(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get local net iface IP address: valid only for Ethernet devices.");
        return "0:0:0:0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->localAddress.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

uint8_t ob_device_list_get_device_local_subnet_length(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("Failed to get local net iface subnet length: valid only for Ethernet devices.");
        return 0;
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->localSubnetLength;
}
HANDLE_EXCEPTIONS_AND_RETURN(uint8_t(0), list, index)

const char *ob_device_list_get_device_local_gateway(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        LOG_WARN("get localGateway() failed! Only valid for Ethernet devices.");
        return "0:0:0:0";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return (netSourcePort->localGateway.empty() || netSourcePort->localGateway == "unknown") ? "0.0.0.0" : netSourcePort->localGateway.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_ip_source_type ob_device_list_get_device_ip_source_type(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        return OB_IP_SOURCE_NONE;
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return static_cast<ob_ip_source_type>(netSourcePort->curIpConfig);
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_IP_SOURCE_NONE, list, index)

const char *ob_device_list_get_device_local_net_if_name(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        return "unknown";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->netInterfaceName.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

const char *ob_device_list_get_device_user_name(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
    if(std::string(info->getConnectionType()) != "Ethernet") {
        return "unknown";
    }

    auto sourcePort    = info->getSourcePortInfoList().front();
    auto netSourcePort = std::dynamic_pointer_cast<const libobsensor::NetSourcePortInfo>(sourcePort);
    return netSourcePort->userName.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index)

ob_device_access_state ob_device_list_query_device_access_state(const ob_device_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    auto &info = list->list[index];
#if defined(BUILD_NET_PAL)
    if(std::string(info->getConnectionType()) != "Ethernet") {
        return OB_DEVICE_ACCESS_STATE_UNSUPPORTED;
    }
    return libobsensor::GvcpCcpController::queryAccessState(info);
#else
    libobsensor::utils::unusedVar(info);
    return OB_DEVICE_ACCESS_STATE_UNSUPPORTED;
#endif
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_DEVICE_ACCESS_STATE_UNKNOWN, list, index)

ob_device_access_state ob_device_list_query_device_access_state_by_serial_number(const ob_device_list *list, const char *serial_number,
                                                                                 ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(serial_number);
    for(auto &info: list->list) {
        if(info->getDeviceSn() == serial_number) {
#if defined(BUILD_NET_PAL)
            if(std::string(info->getConnectionType()) != "Ethernet") {
                return OB_DEVICE_ACCESS_STATE_UNSUPPORTED;
            }
            return libobsensor::GvcpCcpController::queryAccessState(info);
#else
            return OB_DEVICE_ACCESS_STATE_UNSUPPORTED;
#endif
        }
    }
    THROW_INVALID_PARAM_EXCEPTION("Device not found by serial number: " + std::string(serial_number));
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_DEVICE_ACCESS_STATE_UNKNOWN, list, serial_number)
ob_device *ob_device_list_get_device(const ob_device_list *list, uint32_t index, ob_error **error) {
    return ob_device_list_get_device_ex(list, index, OB_DEVICE_DEFAULT_ACCESS, error);
}

ob_device *ob_device_list_get_device_ex(const ob_device_list *list, uint32_t index, ob_device_access_mode accessMode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->list.size());
    VALIDATE_NOT_EQUAL(accessMode, OB_DEVICE_ACCESS_DENIED);
    auto &info      = list->list[index];
    auto  deviceMgr = info->getDeviceManager();
    VALIDATE_NOT_NULL(deviceMgr);

    auto device  = deviceMgr->createDevice(info, accessMode);
    auto impl    = new ob_device();
    impl->device = device;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, index, accessMode)

ob_device *ob_device_list_get_device_by_serial_number(const ob_device_list *list, const char *serial_number, ob_error **error) {
    return ob_device_list_get_device_by_serial_number_ex(list, serial_number, OB_DEVICE_DEFAULT_ACCESS, error);
}

ob_device *ob_device_list_get_device_by_serial_number_ex(const ob_device_list *list, const char *serial_number, ob_device_access_mode accessMode,
                                                         ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(serial_number);
    VALIDATE_NOT_EQUAL(accessMode, OB_DEVICE_ACCESS_DENIED);
    for(auto &info: list->list) {
        if(info->getDeviceSn() == serial_number) {
            auto deviceMgr = info->getDeviceManager();
            VALIDATE_NOT_NULL(deviceMgr);

            auto device  = deviceMgr->createDevice(info, accessMode);
            auto impl    = new ob_device();
            impl->device = device;
            return impl;
        }
    }
    THROW_INVALID_PARAM_EXCEPTION("Device not found by serial number: " + std::string(serial_number));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, serial_number, accessMode)

ob_device *ob_device_list_get_device_by_uid(const ob_device_list *list, const char *uid, ob_error **error) {
    return ob_device_list_get_device_by_uid_ex(list, uid, OB_DEVICE_DEFAULT_ACCESS, error);
}

ob_device *ob_device_list_get_device_by_uid_ex(const ob_device_list *list, const char *uid, ob_device_access_mode accessMode, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_NOT_NULL(uid);
    VALIDATE_NOT_EQUAL(accessMode, OB_DEVICE_ACCESS_DENIED);
    for(auto &info: list->list) {
        auto deviceUid = info->getUid();
        if(deviceUid == uid
#if defined(__linux__)
           // For linux platform, the usb device uid is like "bus-port-dev", try to rm the dev part to match the uid
           || deviceUid.substr(0, deviceUid.find_last_of('-')) == uid
#endif
        ) {
            auto deviceMgr = info->getDeviceManager();
            VALIDATE_NOT_NULL(deviceMgr);

            auto device  = deviceMgr->createDevice(info, accessMode);
            auto impl    = new ob_device();
            impl->device = device;
            return impl;
        }
    }
    THROW_INVALID_PARAM_EXCEPTION("Device not found by UID: " + std::string(uid));
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, list, uid, accessMode)

void ob_delete_device(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    delete device;
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

ob_sensor_list *ob_device_get_sensor_list(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto impl         = new ob_sensor_list();
    impl->device      = device->device;
    impl->sensorTypes = device->device->getSensorTypeList();
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

ob_sensor *ob_device_get_sensor(ob_device *device, ob_sensor_type type, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto sensorTypelist = device->device->getSensorTypeList();
    auto iter           = std::find(sensorTypelist.begin(), sensorTypelist.end(), type);
    if(iter == sensorTypelist.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Sensor not found by type: " + std::to_string(type));
    }
    auto impl    = new ob_sensor();
    impl->device = device->device;
    impl->type   = type;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device, type)

void ob_device_set_int_property(ob_device *device, ob_property_id property_id, int32_t value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

int32_t ob_device_get_int_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device, property_id)

ob_int_property_range ob_device_get_int_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<int32_t>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_int_property_range(), device, property_id)

void ob_device_set_float_property(ob_device *device, ob_property_id property_id, float value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

float ob_device_get_float_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<float>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(0.0f, device, property_id)

ob_float_property_range ob_device_get_float_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<float>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_float_property_range(), device, property_id)

void ob_device_set_bool_property(ob_device *device, ob_property_id property_id, bool value, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->setPropertyValueT(property_id, value, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, value)

bool ob_device_get_bool_property(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return propServer->getPropertyValueT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id)

ob_bool_property_range ob_device_get_bool_property_range(ob_device *device, ob_property_id property_id, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto range      = propServer->getPropertyRangeT<bool>(property_id, libobsensor::PROP_ACCESS_USER);
    return { range.cur, range.max, range.min, range.step, range.def };
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_bool_property_range(), device, property_id)

void ob_device_set_structured_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    bool triggerOffline = false;
    if(property_id == OB_STRUCT_DEVICE_IP_ADDR_CONFIG) {
        VALIDATE_NOT_NULL(data);
        VALIDATE_EQUAL(data_size, sizeof(ob_net_ip_config));
        auto config      = reinterpret_cast<const ob_net_ip_config *>(data);
        auto devInfo     = device->device->getInfo();
        bool allowZeroGW = libobsensor::utils::isAllowZeroGateway(devInfo->vid_, devInfo->pid_);
        if(!libobsensor::utils::checkIpConfig(*config, allowZeroGW)) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid IP configuration");
            return;
        }
        triggerOffline = true;
    }
    else if(property_id == OB_STRUCT_DEVICE_IP_ADDR_CONFIG_V2) {
        VALIDATE_NOT_NULL(data);
        VALIDATE_EQUAL(data_size, sizeof(ob_net_ip_config_v2));
        auto config      = reinterpret_cast<const ob_net_ip_config_v2 *>(data);
        auto devInfo     = device->device->getInfo();
        bool allowZeroGW = libobsensor::utils::isAllowZeroGateway(devInfo->vid_, devInfo->pid_);
        if(!libobsensor::utils::checkIpConfig(*config, allowZeroGW)) {
            THROW_INVALID_PARAM_EXCEPTION("Invalid IP configuration");
            return;
        }
        triggerOffline = true;
    }

    {
        auto                 propServer = device->device->getPropertyServer();
        std::vector<uint8_t> dataVec(data, data + data_size);
        propServer->setStructureData(property_id, dataVec, libobsensor::PROP_ACCESS_USER);
        triggerOffline = propServer->isPropertySupported(OB_PROP_DEVICE_OFFLINE_AFTER_IP_CONFIG_APPLY, libobsensor::PROP_OP_READ, libobsensor::PROP_ACCESS_USER)
                             ? triggerOffline
                             : false;
    }

    if(triggerOffline) {
        auto devInfo = device->device->getInfo();
        auto devMgr  = libobsensor::DeviceManager::getInstance();
        devMgr->triggerDeviceOffline(devInfo->uid_, true);
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

void ob_device_get_structured_data(ob_device *device, ob_property_id property_id, uint8_t *data, uint32_t *data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer   = device->device->getPropertyServer();
    auto firmwareData = propServer->getStructureData(property_id, libobsensor::PROP_ACCESS_USER);

    memcpy(data, firmwareData.data(), firmwareData.size());
    *data_size = static_cast<uint32_t>(firmwareData.size());
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

bool ob_device_is_license_authorization_supported(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devInner = device->device;
    auto manager  = devInner->getComponentT<libobsensor::IDeviceLicenseInfoManager>(libobsensor::OB_DEV_COMPONENT_DEVICE_LICENSE_INFO_MANAGER, false);
    // Always return true if the manager is registered with the device
    return manager ? true : false;
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_device_write_license_info(ob_device *device, const char *license_info_json, uint32_t license_info_json_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(license_info_json);
    if(license_info_json_size == 0) {
        THROW_INVALID_PARAM_EXCEPTION("license_info_json_size is 0");
    }

    auto        devInner = device->device;
    auto        manager  = devInner->getComponentT<libobsensor::IDeviceLicenseInfoManager>(libobsensor::OB_DEV_COMPONENT_DEVICE_LICENSE_INFO_MANAGER, true);
    std::string jsonStr(license_info_json, license_info_json_size);
    manager->writeLicenseInfo(jsonStr);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, license_info_json, license_info_json_size)

void ob_device_read_license_info(const ob_device *device, char *license_info_json, uint32_t license_info_json_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(license_info_json);

    auto devInner     = device->device;
    auto manager      = devInner->getComponentT<libobsensor::IDeviceLicenseInfoManager>(libobsensor::OB_DEV_COMPONENT_DEVICE_LICENSE_INFO_MANAGER, true);
    auto licenseInfo  = manager->getLicenseInfo();
    auto requiredSize = static_cast<uint32_t>(licenseInfo.size() + 1);
    if(license_info_json_size < requiredSize) {
        THROW_INVALID_PARAM_EXCEPTION("license_info_json buffer is too small, required: " + std::to_string(requiredSize));
    }

    std::memcpy(license_info_json, licenseInfo.c_str(), requiredSize);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, license_info_json, license_info_json_size)

void ob_device_clear_license_info(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devInner = device->device;
    auto manager  = devInner->getComponentT<libobsensor::IDeviceLicenseInfoManager>(libobsensor::OB_DEV_COMPONENT_DEVICE_LICENSE_INFO_MANAGER, true);
    manager->clearLicenseInfo();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)
void ob_device_get_recording_session_result(ob_device *device, const char *storage_path, ob_recording_session_result *result,
                                            ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(storage_path);
    VALIDATE_NOT_NULL(result);
    auto manager =
        device->device->getComponentT<libobsensor::RecordingSessionResultManager>(libobsensor::OB_DEV_COMPONENT_RECORDING_SESSION_RESULT_MANAGER, false);
    if(!manager) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Recording session result query is not supported by this device or firmware");
    }
    *result = manager->getRecordingSessionResult(storage_path);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, storage_path, result)

void ob_device_play_audio_file(ob_device *device, const char *file_path, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(file_path);
    auto player = device->device->getComponentT<libobsensor::IAudioPlayer>(libobsensor::OB_DEV_COMPONENT_NETWORK_AUDIO_PLAYER, false);
    if(!player) {
        player = device->device->getComponentT<libobsensor::IAudioPlayer>(libobsensor::OB_DEV_COMPONENT_USB_AUDIO_PLAYER, false);
    }
    if(!player) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Audio playback is not supported by this device or firmware");
    }
    player->playFile(file_path);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, file_path)

void ob_device_stop_audio_playback(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto player = device->device->getComponentT<libobsensor::IAudioPlayer>(libobsensor::OB_DEV_COMPONENT_NETWORK_AUDIO_PLAYER, false);
    if(!player) {
        player = device->device->getComponentT<libobsensor::IAudioPlayer>(libobsensor::OB_DEV_COMPONENT_USB_AUDIO_PLAYER, false);
    }
    if(!player) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Audio playback is not supported by this device or firmware");
    }
    player->stop();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_set_device_user_name(ob_device *device, const char *name, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(name);

    const auto nameLength = std::strlen(name);
    if(nameLength >= kDeviceUserNameBufferSize) {
        THROW_INVALID_PARAM_EXCEPTION("Device user name exceeds 31 bytes");
    }

    std::vector<uint8_t> nameData(kDeviceUserNameBufferSize, 0);
    std::memcpy(nameData.data(), name, nameLength);

    auto propertyServer = device->device->getPropertyServer();
    propertyServer->setStructureData(OB_STRUCT_DEVICE_USER_NAME, nameData, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, name)

uint32_t ob_device_get_device_user_name(const ob_device *device, char *name, uint32_t name_capacity, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto propertyServer = device->device->getPropertyServer();
    auto nameData       = propertyServer->getStructureData(OB_STRUCT_DEVICE_USER_NAME, libobsensor::PROP_ACCESS_USER);
    if(nameData.size() != kDeviceUserNameBufferSize) {
        THROW_INVALID_DATA_EXCEPTION("Invalid device user name payload size");
    }

    const auto terminator = std::find(nameData.begin(), nameData.end(), static_cast<uint8_t>(0));
    if(terminator == nameData.end()) {
        THROW_INVALID_DATA_EXCEPTION("Device user name is not null terminated");
    }
    const auto nameLength = static_cast<uint32_t>(std::distance(nameData.begin(), terminator));

    if(name == nullptr && name_capacity == 0) {
        return nameLength;
    }

    VALIDATE_NOT_NULL(name);
    if(name_capacity < nameLength + 1) {
        THROW_INVALID_PARAM_EXCEPTION("Device user name output buffer is too small");
    }

    std::memcpy(name, nameData.data(), nameLength);
    name[nameLength] = '\0';
    return nameLength;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device, name, name_capacity)

uint32_t ob_device_get_device_model_type(const ob_device *device, char *model_type, uint32_t model_type_capacity, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto propertyServer = device->device->getPropertyServer();
    auto modelTypeData  = propertyServer->getStructureData(OB_STRUCT_DEVICE_MODEL_TYPE, libobsensor::PROP_ACCESS_USER);
    if(modelTypeData.size() != kDeviceModelTypeBufferSize) {
        THROW_INVALID_DATA_EXCEPTION("Invalid device model type payload size");
    }

    const auto terminator = std::find(modelTypeData.begin(), modelTypeData.end(), static_cast<uint8_t>(0));
    if(terminator == modelTypeData.end()) {
        THROW_INVALID_DATA_EXCEPTION("Device model type is not null terminated");
    }
    const auto modelTypeLength = static_cast<uint32_t>(std::distance(modelTypeData.begin(), terminator));

    if(model_type == nullptr && model_type_capacity == 0) {
        return modelTypeLength;
    }

    VALIDATE_NOT_NULL(model_type);
    if(model_type_capacity < modelTypeLength + 1) {
        THROW_INVALID_PARAM_EXCEPTION("Device model type output buffer is too small");
    }

    std::memcpy(model_type, modelTypeData.data(), modelTypeLength);
    model_type[modelTypeLength] = '\0';
    return modelTypeLength;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device, model_type, model_type_capacity)

void ob_device_get_bluetooth_pairing_info(ob_device *device, ob_bluetooth_pairing_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(info);

    auto propertyServer = device->device->getPropertyServer();
    auto pairingData = propertyServer->getStructureData(OB_STRUCT_BLUETOOTH_PAIRING_INFO, libobsensor::PROP_ACCESS_USER);
    *info = parseBluetoothPairingInfo(pairingData);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, info)

void ob_device_clear_bluetooth_pairing(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto propertyServer = device->device->getPropertyServer();
    propertyServer->setPropertyValueT<int32_t>(OB_PROP_BLUETOOTH_CLEAR_PAIRING_INT, kBluetoothClearPairingValue, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_clear_wifi_config(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto propertyServer = device->device->getPropertyServer();
    propertyServer->setPropertyValueT<int32_t>(OB_PROP_CLEAR_WIFI_CONFIG_INT, kClearWifiConfigValue, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_get_raw_data(ob_device *device, ob_property_id property_id, ob_get_data_callback cb, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    propServer->getRawData(
        property_id,
        [=](OBDataTranState state, OBDataChunk *dataChunk) {
            if(cb) {
                cb(state, dataChunk, user_data);
            }
        },
        libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, user_data)

void ob_device_set_raw_data(ob_device *device, ob_property_id property_id, const uint8_t *data, uint32_t data_size,
                            ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    if(data_size > 0) {
        VALIDATE_NOT_NULL(data);
    }
    if(data_size % 2 != 0) {
        THROW_INVALID_PARAM_EXCEPTION("Raw data size must be even");
    }

    std::vector<uint8_t> rawData;
    if(data_size > 0) {
        rawData.assign(data, data + data_size);
    }
    auto propertyServer = device->device->getPropertyServer();
    propertyServer->setRawData(property_id, rawData, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, property_id, data, data_size)

void ob_device_start_firmware_log_export(ob_device *device, ob_fw_log_packet_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);

    auto logManager = device->device->getComponentT<libobsensor::FirmwareLogManager>(libobsensor::OB_DEV_COMPONENT_FW_LOG_MANAGER, false);
    if(!logManager) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Firmware log export is not supported by this device");
    }
    logManager->start([=](const char *fileName, OBFwLogPacketState state, const uint8_t *data, uint32_t dataSize, uint32_t totalFileCount,
                          uint32_t completedFileCount) {
        callback(fileName, state, data, dataSize, totalFileCount, completedFileCount, user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_stop_firmware_log_export(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto logManager = device->device->getComponentT<libobsensor::FirmwareLogManager>(libobsensor::OB_DEV_COMPONENT_FW_LOG_MANAGER, false);
    if(logManager) {
        logManager->stop();
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_get_fw_record_profile_list(ob_device *device, ob_fw_record_stream_profile *profiles, uint32_t *profile_count,
                                          ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(profile_count);

    auto fwRecordProfileManager =
        device->device->getComponentT<libobsensor::IFwRecordProfileManager>(libobsensor::OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, false);
    if(!fwRecordProfileManager) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Firmware recording profile list is not supported by this device");
    }
    fwRecordProfileManager->getProfileList(profiles, profile_count);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, profiles, profile_count)

void ob_device_set_fw_record_profile_list(ob_device *device, const ob_fw_record_stream_profile *profiles, uint32_t profile_count,
                                          ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(profiles);

    auto fwRecordProfileManager =
        device->device->getComponentT<libobsensor::IFwRecordProfileManager>(libobsensor::OB_DEV_COMPONENT_FW_RECORD_PROFILE_MANAGER, false);
    if(!fwRecordProfileManager) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Firmware recording profile list is not supported by this device");
    }
    fwRecordProfileManager->setProfileList(profiles, profile_count);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, profiles, profile_count)

void ob_device_set_custom_metadata_from_json_file(ob_device *device, const char *json_file_path, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(json_file_path);

    auto manager = device->device->getComponentT<libobsensor::ICustomMetadataManager>(libobsensor::OB_DEV_COMPONENT_CUSTOM_METADATA_MANAGER, false);
    if(!manager) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Custom metadata is not supported by this device");
    }
    manager->setFromJsonFile(json_file_path);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, json_file_path)

void ob_device_get_fw_record_mcap_topic_list(ob_device *device, ob_fw_recording_mcap_topic *topics, uint32_t *topic_count,
                                             ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(topic_count);

    auto data = getMcapTopicListData(device->device.get());
    if(data.size() % sizeof(ob_fw_recording_mcap_topic) != 0) {
        THROW_INVALID_DATA_EXCEPTION("Invalid firmware MCAP topic list size");
    }

    const auto actualCount = static_cast<uint32_t>(data.size() / sizeof(ob_fw_recording_mcap_topic));
    auto       topicList   = reinterpret_cast<const ob_fw_recording_mcap_topic *>(data.data());
    validateMcapTopicList(topicList, actualCount);

    if(topics == nullptr) {
        *topic_count = actualCount;
        return;
    }
    if(*topic_count < actualCount) {
        *topic_count = actualCount;
        THROW_INVALID_PARAM_EXCEPTION("MCAP topic output buffer is too small");
    }

    if(actualCount > 0) {
        memcpy(topics, data.data(), data.size());
    }
    *topic_count = actualCount;
}
HANDLE_EXCEPTIONS_NO_RETURN(device, topics, topic_count)

void ob_device_set_fw_record_mcap_topic_list(ob_device *device, const ob_fw_recording_mcap_topic *topics, uint32_t topic_count,
                                             ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    validateMcapTopicList(topics, topic_count);

    auto propertyServer = device->device->getPropertyServer();
    std::vector<uint8_t> data(static_cast<size_t>(topic_count) * sizeof(ob_fw_recording_mcap_topic));
    if(topic_count > 0) {
        memcpy(data.data(), topics, data.size());
    }
    propertyServer->setRawData(OB_RAW_DATA_FW_RECORDING_MCAP_TOPIC_LIST, data, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, topics, topic_count)

void ob_device_get_fw_recording_part_list(ob_device *device, ob_fw_recording_part_entry *part_entries, uint32_t *part_count,
                                          ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(part_count);

    auto entries = libobsensor::getFwRecordingPartList(device->device.get(), libobsensor::PROP_ACCESS_USER);
    const auto actualCount = static_cast<uint32_t>(entries.size());
    if(part_entries == nullptr) {
        *part_count = actualCount;
        return;
    }
    if(*part_count < actualCount) {
        *part_count = actualCount;
        THROW_INVALID_PARAM_EXCEPTION("Firmware recording part list output buffer is too small");
    }
    if(actualCount > 0) {
        memcpy(part_entries, entries.data(), entries.size() * sizeof(entries.front()));
    }
    *part_count = actualCount;
}
HANDLE_EXCEPTIONS_NO_RETURN(device, part_entries, part_count)

void ob_device_write_customer_data(ob_device *device, const void *data, uint32_t data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data);
    auto updater = device->device->getComponentT<libobsensor::FirmwareUpdater>(libobsensor::OB_DEV_COMPONENT_FIRMWARE_UPDATER, true);
    updater->writeCustomerDataExt(reinterpret_cast<const uint8_t *>(data), data_size, error);
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_read_customer_data(ob_device *device, void *data, uint32_t *data_size, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data);
    auto updater = device->device->getComponentT<libobsensor::FirmwareUpdater>(libobsensor::OB_DEV_COMPONENT_FIRMWARE_UPDATER, true);
    updater->readCustomerDataExt(reinterpret_cast<uint8_t *>(data), data_size, error);
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

uint32_t ob_device_get_supported_property_count(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    return static_cast<int>(propServer->getAvailableProperties(libobsensor::PROP_ACCESS_USER).size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

ob_property_item ob_device_get_supported_property_item(const ob_device *device, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto propServer = device->device->getPropertyServer();
    auto properties = propServer->getAvailableProperties(libobsensor::PROP_ACCESS_USER);
    VALIDATE_UNSIGNED_INDEX(index, properties.size());
    return properties.at(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_property_item(), device, index)

bool ob_device_is_property_supported(const ob_device *device, ob_property_id property_id, ob_permission_type permission, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto operationType = libobsensor::PROP_OP_READ_WRITE;
    if(permission == OB_PERMISSION_READ) {
        operationType = libobsensor::PROP_OP_READ;
    }
    else if(permission == OB_PERMISSION_WRITE) {
        operationType = libobsensor::PROP_OP_WRITE;
    }

    auto propServer = device->device->getPropertyServer();
    return propServer->isPropertySupported(property_id, operationType, libobsensor::PROP_ACCESS_USER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, property_id, permission)

bool ob_device_is_global_timestamp_supported(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    return device->device->isComponentExists(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_device_enable_global_timestamp(ob_device *device, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto globalTimestampFilter = device->device->getComponentT<libobsensor::IGlobalTimestampFitter>(libobsensor::OB_DEV_COMPONENT_GLOBAL_TIMESTAMP_FILTER);
    globalTimestampFilter->enable(enable);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, enable)

void ob_device_update_firmware(ob_device *device, const char *path, ob_device_fw_update_callback callback, bool async, void *user_data,
                               ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(path);
    VALIDATE_NOT_NULL(callback);
    device->device->updateFirmwareFromPath(
        path,
        [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) {  //
            callback(status, message, percent, user_data);
        },
        async);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, path, callback, async, user_data)

void ob_device_update_firmware_from_data(ob_device *device, const uint8_t *data, uint32_t data_size, ob_device_fw_update_callback callback, bool async,
                                         void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data);
    VALIDATE_NOT_NULL(callback);
    auto dataVec = std::vector<uint8_t>(data, data + data_size);
    device->device->updateFirmware(
        dataVec,
        [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) {  //
            callback(status, message, percent, user_data);
        },
        async);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, data, data_size, callback, async, user_data)

void ob_device_update_optional_depth_presets(ob_device *device, const char file_path_list[][OB_PATH_MAX], uint8_t path_count,
                                             ob_device_fw_update_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(file_path_list);
    VALIDATE_GE(path_count, 0);
    VALIDATE_NOT_NULL(callback);

    device->device->updateOptionalDepthPresets(
        file_path_list, path_count,
        [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) { callback(status, message, percent, user_data); });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, file_path_list, path_count, callback, user_data)

void ob_device_update_optional_depth_presets_from_data(ob_device *device, const ob_data_view *data_list, uint8_t count,
                                                       ob_device_fw_update_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(data_list);
    VALIDATE_GE(count, 0);
    VALIDATE_NOT_NULL(callback);

    device->device->updateOptionalDepthPresets(data_list, count, [callback, user_data](ob_fw_update_state status, const char *message, uint8_t percent) {
        callback(status, message, percent, user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, data_list, count, callback, user_data)

void ob_device_reboot(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    device->device->reboot();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

ob_device_state ob_device_get_device_state(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);

    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    return devMonitor->getCurrentDeviceState();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void ob_device_set_state_changed_callback(ob_device *device, ob_device_state_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    devMonitor->registerStateChangedCallback([callback, user_data](OBDeviceState state, const std::string &message) {  //
        callback(state, message.c_str(), user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_set_direction_key_callback(ob_device *device, ob_device_direction_key_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);
    auto directionKeyService = device->device->getComponentT<libobsensor::DirectionKeyService>(libobsensor::OB_DEV_COMPONENT_DIRECTION_KEY_SERVICE);
    directionKeyService->setCallback([callback, user_data](const OBDirectionKeyInfomation &info) { callback(&info, user_data); });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_clear_direction_key_callback(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto directionKeyService = device->device->getComponentT<libobsensor::DirectionKeyService>(libobsensor::OB_DEV_COMPONENT_DIRECTION_KEY_SERVICE);
    directionKeyService->clearCallback();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_set_battery_info_callback(ob_device *device, ob_battery_info_callback callback, void *user_data, ob_error **error)
    BEGIN_API_CALL {
        VALIDATE_NOT_NULL(device);
        VALIDATE_NOT_NULL(callback);
        auto batteryInfoService =
            device->device->getComponentT<libobsensor::BatteryInfoService>(libobsensor::OB_DEV_COMPONENT_BATTERY_INFO_SERVICE);
        batteryInfoService->setCallback([callback, user_data](const OBBatteryInfo &info) { callback(&info, user_data); });
    }
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_clear_battery_info_callback(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto batteryInfoService =
        device->device->getComponentT<libobsensor::BatteryInfoService>(libobsensor::OB_DEV_COMPONENT_BATTERY_INFO_SERVICE);
    batteryInfoService->clearCallback();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_set_ego_state_callback(ob_device *device, ob_ego_state_callback callback, void *user_data, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);
    auto egoStateService = device->device->getComponentT<libobsensor::EgoStateService>(libobsensor::OB_DEV_COMPONENT_EGO_STATE_SERVICE);
    egoStateService->setCallback([callback, user_data](const OBEgoStateReport &state) { callback(&state, user_data); });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_clear_ego_state_callback(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto egoStateService = device->device->getComponentT<libobsensor::EgoStateService>(libobsensor::OB_DEV_COMPONENT_EGO_STATE_SERVICE);
    egoStateService->clearCallback();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_get_ego_state(ob_device *device, ob_ego_state_report *state, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(state);
    auto egoStateService = device->device->getComponentT<libobsensor::EgoStateService>(libobsensor::OB_DEV_COMPONENT_EGO_STATE_SERVICE);
    *state = egoStateService->getState();
}
HANDLE_EXCEPTIONS_NO_RETURN(device, state)

void ob_device_set_ego_recording_result_callback(ob_device *device, ob_ego_recording_result_callback callback, void *user_data,
                                                  ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(callback);
    auto egoStateService = device->device->getComponentT<libobsensor::EgoStateService>(libobsensor::OB_DEV_COMPONENT_EGO_STATE_SERVICE);
    egoStateService->setRecordingResultCallback([callback, user_data](const libobsensor::EgoStateService::EgoRecordingResult &result) {
        std::vector<std::array<char, OB_FW_RECORDING_PART_NAME_SIZE>> partNames(result.partNames.size());
        for(size_t index = 0; index < result.partNames.size(); ++index) {
            const auto &name = result.partNames[index];
            const auto copySize = (std::min)(name.size(), static_cast<size_t>(OB_FW_RECORDING_PART_NAME_SIZE - 1));
            if(copySize > 0) {
                memcpy(partNames[index].data(), name.data(), copySize);
            }
        }

        OBEgoRecordingResult cResult{};
        cResult.state               = result.state;
        cResult.recording_directory = result.recordingDirectory.c_str();
        cResult.directory_size      = static_cast<uint32_t>(result.recordingDirectory.size());
        cResult.part_names          = reinterpret_cast<const char (*)[OB_FW_RECORDING_PART_NAME_SIZE]>(partNames.data());
        cResult.part_count          = static_cast<uint32_t>(partNames.size());
        cResult.directory_valid     = result.directoryValid;
        cResult.part_list_valid     = result.partListValid;
        callback(&cResult, user_data);
    });
}
HANDLE_EXCEPTIONS_NO_RETURN(device, callback, user_data)

void ob_device_clear_ego_recording_result_callback(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto egoStateService = device->device->getComponentT<libobsensor::EgoStateService>(libobsensor::OB_DEV_COMPONENT_EGO_STATE_SERVICE);
    egoStateService->clearRecordingResultCallback();
}
HANDLE_EXCEPTIONS_NO_RETURN(device)

void ob_device_enable_heartbeat(ob_device *device, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);

    if(enable) {
        devMonitor->enableHeartbeat();
    }
    else {
        devMonitor->disableHeartbeat();
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(device, enable)

void ob_device_enable_firmware_log(ob_device *device, bool enable, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);

    if(enable) {
        devMonitor->enableFirmwareLog();
    }
    else {
        devMonitor->disableFirmwareLog();
    }
}
HANDLE_EXCEPTIONS_NO_RETURN(device, enable)

bool ob_device_is_firmware_log_enabled(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    return devMonitor->isFirmwareLogEnabled();
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device)

void ob_device_set_monitor_poll_interval(ob_device *device, uint32_t interval_ms, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    devMonitor->setPollInterval(interval_ms);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, interval_ms)

uint32_t ob_device_get_monitor_poll_interval(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    return devMonitor->getPollInterval();
}
HANDLE_EXCEPTIONS_AND_RETURN(0, device)

void ob_device_send_and_receive_data(ob_device *device, const uint8_t *send_data, uint32_t send_data_size, uint8_t *receive_data, uint32_t *receive_data_size,
                                     ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(send_data);
    VALIDATE_NOT_NULL(receive_data);
    VALIDATE_NOT_NULL(receive_data_size);

    auto devMonitor = device->device->getComponentT<libobsensor::IDeviceMonitor>(libobsensor::OB_DEV_COMPONENT_DEVICE_MONITOR);
    devMonitor->sendAndReceiveData(send_data, send_data_size, receive_data, receive_data_size);
}
HANDLE_EXCEPTIONS_NO_RETURN(device, send_data, send_data_size, receive_data, receive_data_size)

ob_device_info *ob_device_get_device_info(const ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto info  = device->device->getInfo();
    auto impl  = new ob_device_info();
    impl->info = info;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

void ob_delete_device_info(ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    delete info;
}
HANDLE_EXCEPTIONS_NO_RETURN(info)

const char *ob_device_info_get_name(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->fullName_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

int ob_device_info_get_pid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->pid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

int ob_device_info_get_vid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->vid_;
}
HANDLE_EXCEPTIONS_AND_RETURN(0, info)

const char *ob_device_info_get_uid(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->uid_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_serial_number(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->deviceSn_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_firmware_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->fwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_hardware_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->hwVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_connection_type(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->connectionType_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_ip_address(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    auto netDeviceInfo = std::static_pointer_cast<const libobsensor::NetDeviceInfo>(info->info);
    if(!netDeviceInfo) {
        THROW_INVALID_PARAM_EXCEPTION("Not a network device");
    }
    return netDeviceInfo->ipAddress_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_supported_min_sdk_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->supportedSdkVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_subsystem_version(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->subSystemVersion_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_asicName(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return info->info->asicName_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

ob_device_type ob_device_info_get_device_type(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    return static_cast<ob_device_type>(info->info->type_);
}
HANDLE_EXCEPTIONS_AND_RETURN(OB_DEVICE_TYPE_UNKNOWN, info)

const char *ob_device_info_get_subnet_mask(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    auto netDeviceInfo = std::static_pointer_cast<const libobsensor::NetDeviceInfo>(info->info);
    if(!netDeviceInfo) {
        THROW_INVALID_PARAM_EXCEPTION("Not a network device");
    }
    return netDeviceInfo->subnetMask_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

const char *ob_device_info_get_gateway(const ob_device_info *info, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(info);
    auto netDeviceInfo = std::static_pointer_cast<const libobsensor::NetDeviceInfo>(info->info);
    if(!netDeviceInfo) {
        THROW_INVALID_PARAM_EXCEPTION("Not a network device");
    }
    return netDeviceInfo->gateway_.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, info)

bool ob_device_is_extension_info_exist(const ob_device *device, const char *info_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(info_key);
    return device->device->isExtensionInfoExists(info_key);
}
HANDLE_EXCEPTIONS_AND_RETURN(false, device, info_key)

const char *ob_device_get_extension_info(const ob_device *device, const char *info_key, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    VALIDATE_NOT_NULL(info_key);
    const auto &extensionInfo = device->device->getExtensionInfo(info_key);
    if(extensionInfo.empty()) {
        return nullptr;
    }
    return extensionInfo.c_str();
}
HANDLE_EXCEPTIONS_AND_RETURN("", device, info_key)

ob_camera_param_list *ob_device_get_calibration_camera_param_list(ob_device *device, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(device);
    auto algParamManager         = device->device->getComponentT<libobsensor::IAlgParamManager>(libobsensor::OB_DEV_COMPONENT_ALG_PARAM_MANAGER, false);
    auto calibrationCameraParams = algParamManager->getCalibrationCameraParamList();
    auto impl                    = new ob_camera_param_list();
    impl->paramList              = calibrationCameraParams;
    return impl;
}
HANDLE_EXCEPTIONS_AND_RETURN(nullptr, device)

uint32_t ob_camera_param_list_get_count(ob_camera_param_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    return static_cast<uint32_t>(list->paramList.size());
}
HANDLE_EXCEPTIONS_AND_RETURN(0, list)

ob_camera_param ob_camera_param_list_get_param(ob_camera_param_list *list, uint32_t index, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    VALIDATE_UNSIGNED_INDEX(index, list->paramList.size());

    return list->paramList.at(index);
}
HANDLE_EXCEPTIONS_AND_RETURN(ob_camera_param(), list, index)

void ob_delete_camera_param_list(ob_camera_param_list *list, ob_error **error) BEGIN_API_CALL {
    VALIDATE_NOT_NULL(list);
    delete list;
}
HANDLE_EXCEPTIONS_NO_RETURN(list)

#ifdef __cplusplus
}
#endif

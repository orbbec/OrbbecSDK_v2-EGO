// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

/**
 * @file DataAcquisitionDevice.hpp
 * @brief Data-acquisition device wrapper for Ego series capabilities.
 *
 * The Ego series (Ego / EgoPro / Wrist) is a wearable multi-camera data
 * acquisition device. It exposes recording, HID and pairing APIs that are not
 * available on traditional depth cameras, so they live in this dedicated
 * @ref ob::DataAcquisitionDevice subclass instead of the base @ref ob::Device.
 */
#pragma once
#include "libobsensor/h/DataAcquisitionDevice.h"
#include "Device.hpp"

namespace ob {

class DataAcquisitionDevice : public Device {
public:
    /**
     * @brief Callback function for device direction key events.
     *
     * @param[in] info Direction key event information.
     */
    typedef std::function<void(const OBDirectionKeyInfomation &info)> DirectionKeyCallback;

    /**
     * @brief Callback function for HID battery information reports.
     *
     * @param[in] info Battery information.
     */
    typedef std::function<void(const OBBatteryInfo &info)> BatteryInfoCallback;

    /**
     * @brief Callback function for Ego HID state reports.
     *
     * @param[in] state Ego device state information.
     */
    typedef std::function<void(const OBEgoStateReport &state)> EgoStateCallback;

    struct EgoRecordingResult {
        OBEgoStateReport         state;
        std::string              recordingDirectory;
        std::vector<std::string> partNames;
        bool                     directoryValid = false;
        bool                     partListValid  = false;
    };

    typedef std::function<void(const EgoRecordingResult &result)> EgoRecordingResultCallback;

    /**
     * @brief Callback function for firmware log export packets.
     */
    typedef std::function<void(const char *fileName, OBFwLogPacketState state, const uint8_t *data, uint32_t dataSize, uint32_t totalFileCount,
                               uint32_t completedFileCount)>
        FirmwareLogPacketCallback;

public:
    /**
     * @brief Wrap an existing device handle as a data-acquisition device.
     */
    explicit DataAcquisitionDevice(ob_device_t *impl) : Device(impl) {}

    /**
     * @brief Adopt an existing base device by moving its ownership.
     */
    explicit DataAcquisitionDevice(Device &&other) noexcept : Device(std::move(other)) {}

    DataAcquisitionDevice(const DataAcquisitionDevice &)            = delete;
    DataAcquisitionDevice &operator=(const DataAcquisitionDevice &) = delete;

    ~DataAcquisitionDevice() noexcept override {
        stopFirmwareLogExportNoThrow();
    }

    /**
     * @brief Adopt an already-created device as a data-acquisition device.
     *
     * Ownership of the underlying device handle is transferred to the returned
     * DataAcquisitionDevice, and the supplied @p device is left in a moved-from state.
     *
     * @note Only use this for Ego-series devices (Ego / EgoPro / Wrist).
     */
    static std::shared_ptr<DataAcquisitionDevice> fromDevice(std::shared_ptr<Device> device) {
        return std::make_shared<DataAcquisitionDevice>(std::move(*device));
    }

    /**
     * @brief Set the directory used for firmware SD-card recordings.
     *
     * @param[in] directory UTF-8 Linux-style directory path.
     * @throw ob::Error if the directory is invalid, firmware recording is active, or the device rejects the setting.
     */
    void setFwRecordingDirectory(const std::string &directory) const {
        setStructuredData(OB_STRUCT_FW_RECORDING_DIRECTORY, reinterpret_cast<const uint8_t *>(directory.data()),
                          static_cast<uint32_t>(directory.size()));
    }

    /**
     * @brief Get the directory currently used for firmware SD-card recordings.
     *
     * @return UTF-8 Linux-style directory path.
     */
    std::string getFwRecordingDirectory() const {
        std::array<uint8_t, OB_PATH_MAX> buffer{};
        uint32_t                          dataSize = 0;
        getStructuredData(OB_STRUCT_FW_RECORDING_DIRECTORY, buffer.data(), &dataSize);
        return std::string(reinterpret_cast<const char *>(buffer.data()), dataSize);
    }

    /**
     * @brief Set the persistent user-defined device name.
     */
    void setDeviceUserName(const std::string &name) const {
        ob_error *error = nullptr;
        ob_device_set_device_user_name(impl_, name.c_str(), &error);
        Error::handle(&error);
    }

    /**
     * @brief Get the persistent user-defined device name.
     */
    std::string getDeviceUserName() const {
        std::array<char, 33> name{};
        ob_error            *error = nullptr;
        const auto length = ob_device_get_device_user_name(impl_, name.data(), static_cast<uint32_t>(name.size()), &error);
        Error::handle(&error);
        return std::string(name.data(), length);
    }

    /**
     * @brief Get the parsed Ego device pairing information.
     *
     * @details Bluetooth names are not null terminated. Use pairing_count and
     * each record's name_length to access the valid records and bytes.
     */
    OBEgoPairingInfo getEgoPairingInfo() const {
        OBEgoPairingInfo info{};
        ob_error        *error = nullptr;
        ob_device_get_ego_pairing_info(impl_, &info, &error);
        Error::handle(&error);
        return info;
    }

    /**
     * @brief Clear all pairing records stored by the Ego device.
     */
    void clearEgoPairing() const {
        ob_error *error = nullptr;
        ob_device_clear_ego_pairing(impl_, &error);
        Error::handle(&error);
    }

    /**
     * @brief Start exporting firmware log files.
     *
     * @param[in] callback Callback to receive each log file packet.
     */
    void startFirmwareLogExport(FirmwareLogPacketCallback callback) {
        ob_error *error = nullptr;
        firmwareLogCallback_ = std::move(callback);
        ob_device_start_firmware_log_export(impl_, &DataAcquisitionDevice::firmwareLogCallbackBridge, this, &error);
        try {
            Error::handle(&error);
        }
        catch(...) {
            firmwareLogCallback_ = nullptr;
            throw;
        }
    }

    /**
     * @brief Stop exporting firmware log files.
     */
    void stopFirmwareLogExport() {
        ob_error *error = nullptr;
        ob_device_stop_firmware_log_export(impl_, &error);
        firmwareLogCallback_ = nullptr;
        Error::handle(&error);
    }

    /**
     * @brief Get firmware recording profile list saved on the device.
     *
     * @return std::vector<OBFwRecordStreamProfile> Firmware recording profile list
     */
    std::vector<OBFwRecordStreamProfile> getFwRecordProfileList() const {
        ob_error *error = nullptr;
        uint32_t  count = 0;
        ob_device_get_fw_record_profile_list(impl_, nullptr, &count, &error);
        Error::handle(&error);

        std::vector<OBFwRecordStreamProfile> profiles(count);
        if(count > 0) {
            ob_device_get_fw_record_profile_list(impl_, profiles.data(), &count, &error);
            Error::handle(&error);
            profiles.resize(count);
        }
        return profiles;
    }

    /**
     * @brief Set firmware recording profile list used by device-side recording.
     *
     * @param[in] profiles Firmware recording profile list
     */
    void setFwRecordProfileList(const std::vector<OBFwRecordStreamProfile> &profiles) const {
        ob_error *error = nullptr;
        ob_device_set_fw_record_profile_list(impl_, profiles.data(), static_cast<uint32_t>(profiles.size()), &error);
        Error::handle(&error);
    }

    /**
     * @brief Set custom device metadata from a JSON file.
     *
     * The SDK parses and compactly serializes the JSON before sending it to
     * the device. The serialized JSON must not exceed 1000 bytes.
     */
    void setCustomMetadataFromJsonFile(const std::string &jsonFilePath) const {
        ob_error *error = nullptr;
        ob_device_set_custom_metadata_from_json_file(impl_, jsonFilePath.c_str(), &error);
        Error::handle(&error);
    }

    /**
     * @brief Get the custom MCAP topic list used by firmware SD-card recording.
     *
     * @return A snapshot of MCAP topic-type to custom-topic mappings configured in firmware.
     */
    std::vector<OBFwRecordingMcapTopic> getFwRecordMcapTopicList() const {
        ob_error *error = nullptr;
        uint32_t  count = 0;
        ob_device_get_fw_record_mcap_topic_list(impl_, nullptr, &count, &error);
        Error::handle(&error);

        std::vector<OBFwRecordingMcapTopic> topics(count);
        if(count > 0) {
            ob_device_get_fw_record_mcap_topic_list(impl_, topics.data(), &count, &error);
            Error::handle(&error);
            topics.resize(count);
        }
        return topics;
    }

    /**
     * @brief Get the part directory names of the most recent firmware recording result.
     */
    std::vector<std::string> getFwRecordingPartList() const {
        ob_error *error = nullptr;
        uint32_t  count = 0;
        ob_device_get_fw_recording_part_list(impl_, nullptr, &count, &error);
        Error::handle(&error);

        std::vector<OBFwRecordingPartEntry> entries(count);
        if(count > 0) {
            ob_device_get_fw_recording_part_list(impl_, entries.data(), &count, &error);
            Error::handle(&error);
            entries.resize(count);
        }

        std::vector<std::string> names;
        names.reserve(entries.size());
        for(const auto &entry : entries) {
            size_t length = 0;
            while(length < sizeof(entry.name) && entry.name[length] != '\0') {
                ++length;
            }
            names.emplace_back(entry.name, length);
        }
        return names;
    }

    /**
     * @brief Replace the custom MCAP topic list used by firmware SD-card recording.
     *
     * @param[in] topics Complete MCAP topic-type to custom-topic mapping. An empty vector clears the list.
     */
    void setFwRecordMcapTopicList(const std::vector<OBFwRecordingMcapTopic> &topics) const {
        if(topics.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()) / sizeof(OBFwRecordingMcapTopic)) {
            throw std::length_error("MCAP topic list is too large for the device protocol");
        }
        ob_error *error = nullptr;
        ob_device_set_fw_record_mcap_topic_list(impl_, topics.data(), static_cast<uint32_t>(topics.size()), &error);
        Error::handle(&error);
    }

    /**
     * @brief Set the direction key callback.
     *
     * @param[in] callback The callback function that is triggered when a HID key event is received.
     */
    void setDirectionKeyCallback(DirectionKeyCallback callback) {
        if(!callback) {
            clearDirectionKeyCallback();
            return;
        }

        ob_error *error       = nullptr;
        directionKeyCallback_ = callback;
        ob_device_set_direction_key_callback(impl_, &DataAcquisitionDevice::directionKeyCallbackBridge, this, &error);
        Error::handle(&error);
    }

    /**
     * @brief Clear the direction key callback.
     */
    void clearDirectionKeyCallback() {
        ob_error *error = nullptr;
        ob_device_clear_direction_key_callback(impl_, &error);
        Error::handle(&error);
        directionKeyCallback_ = nullptr;
    }

    /**
     * @brief Set the HID battery-information callback.
     *
     * @param[in] callback The callback function that is triggered when a HID battery report is received.
     */
    void setBatteryInfoCallback(BatteryInfoCallback callback) {
        if(!callback) {
            clearBatteryInfoCallback();
            return;
        }

        ob_error *error      = nullptr;
        batteryInfoCallback_ = callback;
        try {
            ob_device_set_battery_info_callback(impl_, &DataAcquisitionDevice::batteryInfoCallbackBridge, this, &error);
            Error::handle(&error);
        }
        catch(...) {
            batteryInfoCallback_ = nullptr;
            throw;
        }
    }

    /**
     * @brief Clear the HID battery-information callback.
     */
    void clearBatteryInfoCallback() {
        ob_error *error = nullptr;
        ob_device_clear_battery_info_callback(impl_, &error);
        Error::handle(&error);
        batteryInfoCallback_ = nullptr;
    }

    /**
     * @brief Set the Ego device state callback.
     *
     * @param[in] callback The callback function that is triggered when an Ego HID state report is received.
     */
    void setEgoStateCallback(EgoStateCallback callback) {
        if(!callback) {
            clearEgoStateCallback();
            return;
        }

        ob_error *error    = nullptr;
        egoStateCallback_ = callback;
        ob_device_set_ego_state_callback(impl_, &DataAcquisitionDevice::egoStateCallbackBridge, this, &error);
        Error::handle(&error);
    }

    /**
     * @brief Clear the Ego device state callback.
     */
    void clearEgoStateCallback() {
        ob_error *error = nullptr;
        ob_device_clear_ego_state_callback(impl_, &error);
        Error::handle(&error);
        egoStateCallback_ = nullptr;
    }

    /**
     * @brief Set the callback invoked when a firmware recording result is ready.
     */
    void setEgoRecordingResultCallback(EgoRecordingResultCallback callback) {
        if(!callback) {
            clearEgoRecordingResultCallback();
            return;
        }

        ob_error *error                = nullptr;
        egoRecordingResultCallback_ = std::move(callback);
        ob_device_set_ego_recording_result_callback(impl_, &DataAcquisitionDevice::egoRecordingResultCallbackBridge, this, &error);
        try {
            Error::handle(&error);
        }
        catch(...) {
            egoRecordingResultCallback_ = nullptr;
            throw;
        }
    }

    /**
     * @brief Clear the firmware recording result callback.
     */
    void clearEgoRecordingResultCallback() {
        ob_error *error = nullptr;
        ob_device_clear_ego_recording_result_callback(impl_, &error);
        egoRecordingResultCallback_ = nullptr;
        Error::handle(&error);
    }

private:
    DirectionKeyCallback       directionKeyCallback_;
    BatteryInfoCallback        batteryInfoCallback_;
    EgoStateCallback           egoStateCallback_;
    EgoRecordingResultCallback egoRecordingResultCallback_;
    FirmwareLogPacketCallback  firmwareLogCallback_;

    void stopFirmwareLogExportNoThrow() noexcept {
        if(!impl_) {
            firmwareLogCallback_ = nullptr;
            return;
        }

        ob_error *error = nullptr;
        ob_device_stop_firmware_log_export(impl_, &error);
        firmwareLogCallback_ = nullptr;
        Error::handle(&error, false);
    }

    static void directionKeyCallbackBridge(const OBDirectionKeyInfomation *info, void *userData) {
        auto device = static_cast<DataAcquisitionDevice *>(userData);
        if(device && device->directionKeyCallback_ && info) {
            device->directionKeyCallback_(*info);
        }
    }

    static void batteryInfoCallbackBridge(const OBBatteryInfo *info, void *userData) {
        auto device = static_cast<DataAcquisitionDevice *>(userData);
        if(device && device->batteryInfoCallback_ && info) {
            device->batteryInfoCallback_(*info);
        }
    }

    static void egoStateCallbackBridge(const OBEgoStateReport *state, void *userData) {
        auto device = static_cast<DataAcquisitionDevice *>(userData);
        if(device && device->egoStateCallback_ && state) {
            device->egoStateCallback_(*state);
        }
    }

    static void egoRecordingResultCallbackBridge(const OBEgoRecordingResult *result, void *userData) {
        auto device = static_cast<DataAcquisitionDevice *>(userData);
        if(!device || !device->egoRecordingResultCallback_ || !result) {
            return;
        }

        EgoRecordingResult cppResult{};
        cppResult.state          = result->state;
        cppResult.directoryValid = result->directory_valid;
        cppResult.partListValid  = result->part_list_valid;
        if(result->directory_valid && result->recording_directory) {
            cppResult.recordingDirectory.assign(result->recording_directory, result->directory_size);
        }
        if(result->part_list_valid && result->part_names) {
            cppResult.partNames.reserve(result->part_count);
            for(uint32_t index = 0; index < result->part_count; ++index) {
                cppResult.partNames.emplace_back(result->part_names[index]);
            }
        }
        device->egoRecordingResultCallback_(cppResult);
    }

    static void firmwareLogCallbackBridge(const char *fileName, ob_fw_log_packet_state state, const uint8_t *data, uint32_t dataSize,
                                          uint32_t totalFileCount, uint32_t completedFileCount, void *userData) {
        auto device = static_cast<DataAcquisitionDevice *>(userData);
        if(device && device->firmwareLogCallback_) {
            device->firmwareLogCallback_(fileName, state, data, dataSize, totalFileCount, completedFileCount);
        }
    }
};

}  // namespace ob

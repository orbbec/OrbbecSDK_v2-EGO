// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

/**
 * @file DataAcquisitionDevice.h
 * @brief C APIs for data-acquisition devices (Ego / EgoPro / Wrist).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "ObTypes.h"

/**
 * @brief Set the persistent user-defined device name.
 *
 * The device receives a 32-byte null-terminated buffer.
 */
OB_EXPORT void ob_device_set_device_user_name(ob_device *device, const char *name, ob_error **error);

/**
 * @brief Get the persistent user-defined device name.
 *
 * @return Name length excluding the trailing null character.
 */
OB_EXPORT uint32_t ob_device_get_device_user_name(const ob_device *device, char *name, uint32_t name_capacity, ob_error **error);

/**
 * @brief Get Bluetooth pairing information stored by the device.
 *
 * @details The returned Bluetooth names are not null terminated. Use
 * @ref ob_bluetooth_pairing_info::paired_device_count and each record's @ref name_length
 * to access the valid records and bytes.
 *
 * @param[in] device The device object.
 * @param[out] info The parsed pairing information.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_get_bluetooth_pairing_info(ob_device *device, ob_bluetooth_pairing_info *info, ob_error **error);

/**
 * @brief Clear all Bluetooth pairing records stored by the device.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_clear_bluetooth_pairing(ob_device *device, ob_error **error);

/**
 * @brief Clear saved Wi-Fi provisioning information on an Ego or Wrist device.
 *
 * The firmware removes persisted Wi-Fi credentials and disables automatic
 * reconnection after reboot. This call only affects the specified device.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_clear_wifi_config(ob_device *device, ob_error **error);

/**
 * @brief Start exporting firmware log files.
 *
 * @param[in] device The device object.
 * @param[in] callback Callback to receive each log file packet.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_start_firmware_log_export(ob_device *device, ob_fw_log_packet_callback callback, void *user_data, ob_error **error);

/**
 * @brief Stop exporting firmware log files.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_stop_firmware_log_export(ob_device *device, ob_error **error);

/**
 * @brief Get the firmware recording profile list saved on the device.
 *
 * @param[in] device The device object.
 * @param[out] profiles Output profile buffer. Set to NULL to query the required profile count.
 * @param[in,out] profile_count Input buffer capacity when profiles is not NULL; output actual profile count.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_get_fw_record_profile_list(ob_device *device, ob_fw_record_stream_profile *profiles, uint32_t *profile_count, ob_error **error);

/**
 * @brief Set the firmware recording profile list used by device-side recording.
 *
 * @param[in] device The device object.
 * @param[in] profiles Profile list to save to firmware.
 * @param[in] profile_count Profile count.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_fw_record_profile_list(ob_device *device, const ob_fw_record_stream_profile *profiles, uint32_t profile_count,
                                                    ob_error **error);

/**
 * @brief Set custom device metadata from a JSON file.
 *
 * @details The SDK parses the file and sends compact UTF-8 JSON to the
 *          device. The serialized JSON content must not exceed 1000 bytes.
 *          Supported only by Ego, EgoPro, and Wrist series devices.
 *
 * @param[in] device Device object.
 * @param[in] json_file_path JSON file path.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_custom_metadata_from_json_file(ob_device *device, const char *json_file_path, ob_error **error);

/**
 * @brief Get the custom MCAP topic list used by firmware SD-card recording.
 *
 * @param[in] device The device object.
 * @param[out] topics Output topic buffer. Set to NULL to query the required topic count.
 * @param[in,out] topic_count Input buffer capacity when topics is not NULL; output actual topic count.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 *
 * @details Each entry maps one @ref ob_fw_recording_mcap_topic_type value to a custom UTF-8 MCAP topic.
 */
OB_EXPORT void ob_device_get_fw_record_mcap_topic_list(ob_device *device, ob_fw_recording_mcap_topic *topics, uint32_t *topic_count,
                                                        ob_error **error);

/**
 * @brief Set the complete custom MCAP topic list used by firmware SD-card recording.
 *
 * @param[in] device The device object.
 * @param[in] topics Topic entries keyed by MCAP topic type. Set to NULL only when topic_count is zero.
 * @param[in] topic_count Number of topic entries. A zero count clears the custom topic list.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 *
 * @details This operation replaces the complete list. Each topic type must occur once at most;
 *          every topic must be non-empty and NUL-terminated within its 128-byte field.
 */
OB_EXPORT void ob_device_set_fw_record_mcap_topic_list(ob_device *device, const ob_fw_recording_mcap_topic *topics, uint32_t topic_count,
                                                        ob_error **error);

/**
 * @brief Get the part-name list of the most recent firmware recording result.
 *
 * @param[in] device The device object.
 * @param[out] part_entries Output buffer. Set to NULL to query the required count.
 * @param[in,out] part_count Input buffer capacity when part_entries is not NULL; output actual count.
 * @param[out] error Pointer to an error object that will be set if the property is unsupported, the transfer fails, or the payload is invalid.
 */
OB_EXPORT void ob_device_get_fw_recording_part_list(ob_device *device, ob_fw_recording_part_entry *part_entries, uint32_t *part_count,
                                                     ob_error **error);

/**
 * @brief Set the direction key callback for devices that report key events over HID.
 *
 * @param[in] device The device object.
 * @param[in] callback The callback function to be called when a direction key event is received.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_direction_key_callback(ob_device *device, ob_device_direction_key_callback callback, void *user_data, ob_error **error);

/**
 * @brief Clear the direction key callback.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_clear_direction_key_callback(ob_device *device, ob_error **error);

/**
 * @brief Set the HID battery-information callback.
 *
 * This callback is available only on devices that report battery information over HID.
 *
 * @param[in] device The device object.
 * @param[in] callback The callback function to be called when a battery report is received.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_battery_info_callback(ob_device *device, ob_battery_info_callback callback, void *user_data, ob_error **error);

/**
 * @brief Clear the HID battery-information callback.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_clear_battery_info_callback(ob_device *device, ob_error **error);

/**
 * @brief Set the Ego device state callback.
 *
 * @param[in] device The device object.
 * @param[in] callback The callback function to be called when an Ego state report is received.
 * @param[in] user_data User-defined data that will be returned in the callback.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_ego_state_callback(ob_device *device, ob_ego_state_callback callback, void *user_data, ob_error **error);

/**
 * @brief Clear the Ego device state callback.
 *
 * @param[in] device The device object.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_clear_ego_state_callback(ob_device *device, ob_error **error);

/**
 * @brief Set the callback invoked when an Ego firmware recording result is ready.
 */
OB_EXPORT void ob_device_set_ego_recording_result_callback(ob_device *device, ob_ego_recording_result_callback callback, void *user_data,
                                                            ob_error **error);

/**
 * @brief Clear the Ego firmware recording result callback.
 */
OB_EXPORT void ob_device_clear_ego_recording_result_callback(ob_device *device, ob_error **error);

/**
 * @brief Play a WAV audio file through the EGO device speaker.
 *
 * WAV format requirements:
 *   - Encoding:     PCM (uncompressed, format tag 1)
 *   - Channels:     Mono (1 channel)
 *   - Bit depth:    16-bit little-endian
 *   - Sample rate:  16000 Hz
 *
 * This call blocks until playback finishes or ob_device_stop_audio_playback() is called from another thread.
 * Multiple consecutive calls are supported without calling stop in between; the output session
 * remains enabled across calls and is only closed when ob_device_stop_audio_playback() is called.
 */
OB_EXPORT void ob_device_play_audio_file(ob_device *device, const char *file_path, ob_error **error);

/** @brief Stop the active EGO audio playback and close its output session. */
OB_EXPORT void ob_device_stop_audio_playback(ob_device *device, ob_error **error);

#ifdef __cplusplus
}
#endif

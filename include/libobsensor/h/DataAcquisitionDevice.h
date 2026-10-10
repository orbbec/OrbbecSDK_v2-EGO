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
 * @brief Synchronize an Ego-series device clock with an external time source.
 *
 * The external time is sent through @ref OB_STRUCT_DEVICE_TIME. The device
 * firmware applies the time according to its external synchronization policy.
 * The time unit is microseconds and the round-trip-time compensation is zero.
 *
 * @param[in] device The Ego-series data-acquisition device.
 * @param[in] external_time_us External time in microseconds.
 * @param[out] error Pointer to an error object that will be set if the device
 *                   does not support writing device time or the transfer fails.
 */
OB_EXPORT void ob_device_sync_external_time(ob_device *device, uint64_t external_time_us, ob_error **error);

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
 * @brief Get the device model type.
 *
 * The device returns a fixed 32-byte null-terminated UTF-8 buffer.
 *
 * @return Model type length excluding the trailing null character.
 */
OB_EXPORT uint32_t ob_device_get_device_model_type(const ob_device *device, char *model_type, uint32_t model_type_capacity, ob_error **error);

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
 * @brief Set the Wi-Fi name and password on an Ego-series device.
 *
 * @param[in] device The device object.
 * @param[in] config Fixed-size Wi-Fi configuration to write.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_set_wifi_config(ob_device *device, const ob_wifi_config *config, ob_error **error);

/**
 * @brief Get the Wi-Fi name and password from an Ego-series device.
 *
 * @param[in] device The device object.
 * @param[out] config Fixed-size Wi-Fi configuration read from the device.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_get_wifi_config(ob_device *device, ob_wifi_config *config, ob_error **error);

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
 * @brief Query a device-side recording session by its storage path.
 *
 * @details Each call sends the path to the device and then reads the result
 *          from the device control channel. No SDK recording-session cache is
 *          used. This API is supported only by applicable Ego-series devices.
 *
 * @param[in] device The data-acquisition device object.
 * @param[in] storage_path UTF-8 recording task directory supplied when the session was created.
 * @param[out] result The current device-side recording session result.
 * @param[out] error Pointer to an error object that will be set if the query fails.
 */
OB_EXPORT void ob_device_get_recording_session_result(ob_device *device, const char *storage_path, ob_recording_session_result *result,
                                                       ob_error **error);

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
 * @brief Read the current Ego device state directly from the device.
 *
 * This API returns the same @ref ob_ego_state_report type as the Ego state
 * callback, but does not require registering a callback.
 *
 * @param[in] device The device object.
 * @param[out] state Current Ego device state.
 * @param[out] error Pointer to an error object that will be set if an error occurs.
 */
OB_EXPORT void ob_device_get_ego_state(ob_device *device, ob_ego_state_report *state, ob_error **error);

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

// ---- SD-card file service (EGO network device) --------------------------

/**
 * @brief Query whether a path exists on the device SD card.
 *
 * @details Sends GET /api/fs/stat to the device HTTP file service (port 8080).
 *          The path is relative to /mnt/sdcard and must not be empty, contain "..";
 *          start with "/", or contain special characters.
 *
 *          If the path does not exist on the device (firmware HTTP 404),
 *          @p info.exists is set to false and @p error is NOT set.
 *          @p error is set only for communication failures or an invalid path
 *          format (firmware HTTP 400 / 500).
 *
 * @param[in]  device  The device object.
 * @param[in]  path    SD-card-relative path, e.g. "EGO_E0G677200V6_20260915_164720".
 * @param[out] info    Populated on success; info.exists=false when path not found.
 * @param[out] error   Set if the request fails or the path format is invalid.
 */
OB_EXPORT void ob_device_get_sdcard_file_info(ob_device *device, const char *path, ob_device_sdcard_file_info *info, ob_error **error);

/**
 * @brief List the contents of a directory on the device SD card.
 *
 * @details Sends GET /api/fs/list to the device HTTP file service (port 8080).
 *          Pass @p entries as NULL on the first call to obtain the required
 *          buffer size via @p entry_count, then allocate and call again.
 *          The firmware caps the result at 5000 entries; if the actual count
 *          exceeds that limit the result is silently truncated by the firmware.
 *
 * @param[in]     device       The device object.
 * @param[in]     path         SD-card-relative path; NULL or "" for root.
 * @param[in]     recursive    Non-zero to recurse into sub-directories.
 * @param[out]    entries      Caller-allocated buffer; may be NULL (count-only query).
 * @param[in,out] entry_count  Capacity on input (ignored when entries is NULL);
 *                             actual count on output.
 * @param[out]    error        Set if the request fails.
 */
OB_EXPORT void ob_device_get_sdcard_file_list(ob_device *device, const char *path, int recursive, ob_device_sdcard_file_entry *entries,
                                               uint32_t *entry_count, ob_error **error);

/**
 * @brief Download a file from the device SD card, or push it to a remote URL (non-blocking).
 *
 * @details This function returns immediately. Progress, completion, and errors are
 *          reported exclusively through @p callback. Only one download/upload operation
 *          may be active at a time; a second call while one is in progress sets an error.
 *
 *          OB_SDCARD_DOWNLOAD_TO_LOCAL:
 *            SD card file -> @p destination (local file path on the host machine).
 *            Supports HTTP Range resume when the local file already exists.
 *            Only single files are supported. To download a directory, call
 *            ob_device_get_sdcard_file_list() to enumerate files, then call
 *            this function for each file individually.
 *            @p percent in callback: 0~100.
 *
 *          OB_SDCARD_DOWNLOAD_TO_URL:
 *            Device initiates an HTTP PUT to @p destination (remote URL). Both
 *            http:// and https:// destinations are supported by FileService
 *            firmware with TLS support. HTTPS negotiation is performed on the
 *            device side; availability therefore depends on the device firmware,
 *            network connectivity, and device time being correctly set.
 *            @p percent in callback: real-time percent from firmware.
 *
 * @param[in]  device           The device object.
 * @param[in]  device_path      SD-card-relative path.
 * @param[in]  destination_type OB_SDCARD_DOWNLOAD_TO_LOCAL or OB_SDCARD_DOWNLOAD_TO_URL.
 * @param[in]  destination      Local file path (TO_LOCAL) or remote HTTP/HTTPS URL (TO_URL).
 * @param[in]  callback         Optional progress callback; may be NULL.
 * @param[in]  user_data        Passed as-is to @p callback; may be NULL.
 * @param[in]  async            false = block until complete; true = return immediately, result via callback.
 *                               Multiple concurrent async calls are allowed and operate independently.
 * @param[out] error            Set if the operation cannot be started (invalid params, component unavailable).
 *                               Runtime errors are reported through the callback when async=true.
 */
OB_EXPORT void ob_device_download_sdcard_file(ob_device *device, const char *device_path,
                                               ob_sdcard_download_destination destination_type,
                                               const char *destination,
                                               ob_device_sdcard_download_progress_callback callback,
                                               void *user_data, bool async, ob_error **error);

/**
 * @brief Delete a file or directory from the device SD card.
 *
 * @details Sends POST /api/fs/delete to the device HTTP file service (port 8080).
 *          Directories are deleted recursively on the device side.
 *
 * @param[in]  device  The device object.
 * @param[in]  path    SD-card-relative path to delete.
 * @param[out] error   Set if the request fails or the path does not exist.
 */
OB_EXPORT void ob_device_delete_sdcard_file(ob_device *device, const char *path, ob_error **error);

/**
 * @brief Cancel the active SD-card file-service operation on this device.
 *
 * @details Signals the in-progress ob_device_download_sdcard_file() or any
 *          other SD-card file service call (on another thread) to abort.
 *          The blocked caller receives an error with message "operation cancelled".
 *          Safe to call from any thread; no-op when no operation is running.
 *
 * @param[in]  device  The device object.
 * @param[out] error   Set only on internal failure (component not available).
 */
OB_EXPORT void ob_device_cancel_sdcard_file_operation(ob_device *device, ob_error **error);

// ---- end SD-card file service -------------------------------------------

#ifdef __cplusplus
}
#endif

/**
 * @file CamParamsAPI.h
 * @brief High-level convenience API for camera parameter queries.
 *
 * This is the primary interface for firmware and SDK integrators.
 */

#pragma once

#include "ConvertJsonValidate.h"
#include "ParamTransform.h"
#include "YamlSerialization.h"

#include <limits>
#include <string>
#include <vector>

namespace camadapt {

/**
 * @brief Get camera parameters for a specific camera ID and resolution.
 *
 * Searches the ConvertConfig for the given camera ID, matches the output
 * resolution (width × height) to a mode, reads the original calibration
 * from CalibrationData, optionally applies temperature compensation to the
 * focal length, and calls computeCameraParams().
 *
 * Temperature compensation (design doc §3.7): when runtimeTempC is finite, the
 * calibration intrinsics' fx/fy are adjusted by
 *   F' = F + (runtimeTempC - calibTempC) * temp_coeff
 * BEFORE the resolution pipeline (so the compensation scales with the mode).
 * runtimeTempC = NaN (default) disables compensation. Throws
 * std::invalid_argument if temp_coeff != 0 but the calibration temperature is
 * missing for this camera in params.yaml.
 *
 * @param config       convert.json configuration.
 * @param calib        Original calibration data (params.yaml).
 * @param cameraId     Camera ID to query.
 * @param width        Desired output width.
 * @param height       Desired output height.
 * @param runtimeTempC Runtime temperature in °C; NaN (default) = no compensation.
 * @return CameraParams at the target resolution.
 * @throws std::invalid_argument if the camera or the mode is not found, if the camera's
 *         calibration is missing, if temperature data is inconsistent (temp_coeff != 0
 *         without a calibration temperature, §3.7), or if the calibration window cannot be
 *         anchored into the sensor frame (larger than the sensor, or an odd per-axis
 *         difference — the throwing counterpart of validateCalibrationSensor()'s error 61,
 *         §4.10).
 */
CameraParams getCameraParams(const ConvertConfig &config, const CalibrationData &calib, const std::string &cameraId, int width, int height, double runtimeTempC = std::numeric_limits<double>::quiet_NaN());

/**
 * @brief Get camera parameters for ALL cameras at the same output resolution.
 *
 * Convenience batch wrapper: iterates every camera in config (in config order),
 * matches the output resolution (width × height) to each camera's mode, reads
 * the original calibration from CalibrationData, applies temperature
 * compensation (see the single-camera overload), and assembles the adapted
 * CameraParams for every camera into one CalibrationData — ready to be
 * serialized into a single YAML via toYaml()/saveYamlFile().
 *
 * ⚠️ All-or-nothing: EVERY camera must support (width × height), otherwise the whole
 * call throws. A device may mix cameras with different resolutions/mode sets (§1.2
 * requirement 2), so for those devices query first with
 * listCamerasSupportingResolution() and then call the single-camera overload per
 * supported id, merging the results into one CalibrationData before serializing:
 * @code
 *   camadapt::CalibrationData out;
 *   for(const std::string &id : camadapt::listCamerasSupportingResolution(config, w, h)) {
 *       out.cameras.push_back(camadapt::getCameraParams(config, calib, id, w, h, tempC));
 *   }
 *   camadapt::saveYamlFile(path, out);   // cameras that do not support w×h are simply absent
 * @endcode
 *
 * @param config       convert.json configuration.
 * @param calib        Original calibration data (params.yaml).
 * @param width        Desired output width (same for all cameras).
 * @param height       Desired output height (same for all cameras).
 * @param runtimeTempC Runtime temperature in °C; NaN (default) = no compensation.
 * @return CalibrationData with one adapted CameraParams per camera (config order).
 * @throws std::invalid_argument if any camera or mode is not found (all-or-nothing).
 */
CalibrationData getCameraParams(const ConvertConfig &config, const CalibrationData &calib, int width, int height, double runtimeTempC = std::numeric_limits<double>::quiet_NaN());

/**
 * @brief List all supported resolutions for a camera.
 *
 * @param config    convert.json configuration.
 * @param cameraId  Camera ID to query.
 * @return List of supported output sizes.
 * @throws std::invalid_argument if camera not found.
 */
std::vector<Size> listSupportedResolutions(const ConvertConfig &config, const std::string &cameraId);

/**
 * @brief List the cameras that offer a given output resolution (config order).
 *
 * Pure query, never throws (an empty result simply means no camera offers it). Use it
 * before the single-camera getCameraParams() overload when the device may mix cameras
 * with different resolutions/mode sets (§1.2 requirement 2) — it keeps "this camera has
 * no such mode" out of the exception path and lets the caller report which cameras were
 * skipped.
 *
 * @param config  convert.json configuration.
 * @param width   Desired output width.
 * @param height  Desired output height.
 * @return Camera ids (as in convert.json) whose modes contain width × height.
 */
std::vector<std::string> listCamerasSupportingResolution(const ConvertConfig &config, int width, int height);

/**
 * @brief Get the algorithm library version string.
 *
 * Version history (see 工程结构与说明.md §3 for the version ↔ format_version mapping):
 *   2.0.0  initial delivered API
 *   2.1.0  per-camera SensorSizeConfig hand-in, body_fingerprint (tamper detection),
 *          format_version support window (error code 63), listCamerasSupportingResolution()
 *
 * @return Version string, e.g. "2.1.0".
 */
std::string getAlgorithmVersion();

}  // namespace camadapt
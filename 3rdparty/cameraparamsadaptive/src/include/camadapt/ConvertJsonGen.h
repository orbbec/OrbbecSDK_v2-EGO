/**
 * @file ConvertJsonGen.h
 * @brief convert.json generation — config files for the algorithm team, params files for production.
 *
 * convert_config.json (file_type "config") is maintained manually/offline by the
 * algorithm team via generateConvertJson()/appendMode(). convert_params.json
 * (file_type "params") is produced at calibration time by generateParamsJson() with
 * the adaptive crop computation and the semantic fingerprint.
 */

#pragma once

#include "FileIo.h"
#include "ImageTransform.h"
#include "Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace camadapt {

/**
 * @brief Generate a complete convert_config.json (firmware zone) for the algorithm team.
 *
 * For each camera spec and each target resolution, computes the optimal
 * crop/scale parameters to achieve the desired output. The result carries
 * file_type = "config" and an empty fingerprint (design doc §8.2).
 *
 * @param cameraSpecs  Camera sensor specifications.
 * @param resolutions  Target output resolutions.
 * @return Complete ConvertConfig for all cameras.
 */
ConvertConfig generateConvertJson(const std::vector<CameraConfig> &cameraSpecs, const std::vector<Size> &resolutions);

/**
 * @brief Append a new resolution mode to an existing convert.json (algorithm team use).
 *
 * For each camera in the existing config, computes the optimal parameters
 * for the new resolution and appends the mode.
 *
 * @param existing       Existing ConvertConfig.
 * @param newResolution  New resolution to add.
 * @return Updated ConvertConfig with the new mode appended.
 */
ConvertConfig appendMode(const ConvertConfig &existing, const Size &newResolution);

/**
 * @brief Compute the semantic fingerprint of a config (design doc §3.4).
 *
 * Hashes the semantic fields that must not change behind the firmware's back:
 * format_version, and for every camera its id/name/sensor_width/sensor_height,
 * and for every mode its name/width/height/crop_x/crop_y/crop_width/crop_height/
 * rotate/mirror/flip/skipping/binning/lock. scale_x/scale_y (floating point),
 * description and fingerprint itself are excluded. Deterministic FNV-1a 64-bit.
 *
 * @param config  Config to hash.
 * @return Hex-encoded 64-bit fingerprint string.
 */
std::string computeFingerprint(const ConvertConfig &config);

/**
 * @brief Generate a convert_params.json from a convert_config.json (design doc §8.3).
 *
 * The params file is a copy of the config with file_type = "params" and the
 * fingerprint filled in, whose unlock=false modes have been adapted:
 *   1. Base modes (scale==1.0 && skipping==0 && binning==0) with lock=false
 *      recompute crop_x/crop_y by centering the calibration principal point.
 *   2. Derived modes (the rest) with lock=false match a base mode by
 *      (crop size + orientation) and inherit its crop_x/crop_y.
 *
 * @param config             Parsed convert_config.json (file_type "config").
 * @param calib              Calibration data used to centre the principal point.
 * @param actualSensorSizes  Actual sensor resolution of **every** camera, keyed by id.
 *                           A device may mix sensor resolutions, so the declared
 *                           sensor_size is checked per camera and the crop is clamped
 *                           against that camera's own sensor (design doc §8.3).
 * @return ConvertConfig with file_type "params", fingerprint, and adapted crops.
 * @throws std::invalid_argument on missing cameras/modes, on a camera whose actual
 *         resolution is not reported or differs from the declared one, or on an
 *         inadmissible calibration window.
 */
ConvertConfig generateParamsJson(const ConvertConfig &config, const CalibrationData &calib,
                                 const SensorSizeConfig &actualSensorSizes);

/**
 * @brief Generate a convert_params.json for a device whose cameras share one sensor size.
 *
 * Convenience overload for the homogeneous case: it is equivalent to handing in
 * @p sensorSize for every camera of @p config.
 *
 * @param config       Parsed convert_config.json (file_type "config").
 * @param calib       Calibration data used to centre the principal point.
 * @param sensorSize  Actual sensor resolution; must equal the declared sensor_size.
 * @return ConvertConfig with file_type "params", fingerprint, and adapted crops.
 * @throws std::invalid_argument on missing cameras/modes or sensor mismatch.
 */
ConvertConfig generateParamsJson(const ConvertConfig &config, const CalibrationData &calib, const Size &sensorSize);

/**
 * @brief Serialize a ConvertConfig to its flat convert.json representation.
 *
 * The layout matches the design doc §3.1: top-level format_version/file_type/
 * fingerprint/description/cameras; each mode carries the flat fields width/height/
 * crop_x/crop_y/crop_width/crop_height/scale_x/scale_y/rotate/mirror/flip/
 * skipping/binning/lock. Used by the parameter tool to write convert_config.json /
 * convert_params.json.
 *
 * @param config  Config to serialize.
 * @param pretty  Pretty-print (2-space indent) when true.
 * @return JSON string.
 */
std::string toJsonString(const ConvertConfig &config, bool pretty = true);

/**
 * @brief Serialize a ConvertConfig and write it to a file.
 *
 * Host-side convenience: serializes via @ref toJsonString, then writes with
 * @ref writeTextFile. Used to produce convert_config.json (algorithm team) and
 * convert_params.json (calibration time / firmware fallback).
 *
 * @param path    Output file path.
 * @param config  Config to serialize.
 * @param pretty  Pretty-print (2-space indent) when true.
 * @return true on success, false on write failure.
 */
bool saveConvertConfigFile(const std::string &path, const ConvertConfig &config, bool pretty = true);

}  // namespace camadapt
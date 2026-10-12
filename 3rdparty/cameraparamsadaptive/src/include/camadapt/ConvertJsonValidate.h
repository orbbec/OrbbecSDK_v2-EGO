/**
 * @file ConvertJsonValidate.h
 * @brief convert.json validation — JSON format, field values, and consistency checks.
 */

#pragma once

#include "FileIo.h"
#include "ImageTransform.h"
#include "Types.h"

#include <string>

namespace camadapt {

/**
 * @brief Validate a convert.json string comprehensively.
 *
 * Performs JSON format validation, field value validation, and consistency
 * checks in sequence. Returns the first error encountered.
 *
 * @param jsonContent  Raw JSON string content of convert.json.
 * @return ValidateResult with valid=true if all checks pass.
 */
ValidateResult validateConvertJson(const std::string &jsonContent);

/**
 * @brief Validate JSON syntax/format only.
 *
 * @param jsonContent  Raw JSON string content.
 * @return ValidateResult with valid=true if the JSON is well-formed.
 */
ValidateResult validateJsonFormat(const std::string &jsonContent);

/**
 * @brief Parse a convert.json string into a ConvertConfig.
 *
 * Reads the flat field layout of design doc §3.1 (file_type, fingerprint, and
 * the mode-level lock included). The caller normally validates the content first.
 *
 * @param jsonContent  Raw JSON string content.
 * @return Parsed ConvertConfig.
 * @throws std::invalid_argument on malformed JSON.
 */
ConvertConfig parseConvertJson(const std::string &jsonContent);

/**
 * @brief Validate field values in a parsed ConvertConfig.
 *
 * Checks: width > 0, height > 0, rotate ∈ {0, 90, 180, 270}, crop within
 * the sensor bounds, output resolutions unique per camera, mode names unique,
 * camera ids unique (duplicate ids would be collapsed by the id-keyed
 * SensorSizeConfig / ModuleIdConfig), file_type ∈ {"config", "params"},
 * lock is a bool, etc.
 *
 * @param config  Parsed ConvertConfig.
 * @return ValidateResult with valid=true if all fields are valid.
 */
ValidateResult validateFieldValues(const ConvertConfig &config);

/**
 * @brief Validate the consistency equation for each mode (design doc §3.6).
 *
 * Verifies: width = round(crop_width × eff_scale_x) and
 *           height = round(crop_height × eff_scale_y)
 * where eff_scale = scale × skipping_factor × binning_factor, with the
 * width/height swap applied for 90°/270° rotations, and the crop staying
 * within the sensor bounds.
 *
 * @param config  Parsed ConvertConfig.
 * @return ValidateResult with valid=true if all modes are consistent.
 */
ValidateResult validateConsistency(const ConvertConfig &config);

/**
 * @brief Load and parse a convert.json from a file path.
 *
 * Host-side convenience for reading the firmware-zone convert_config.json (or a
 * stored convert_params.json): reads the file via @ref readTextFile, then parses
 * with @ref parseConvertJson. The content is parsed with comment tolerance, so
 * the algorithm team's annotated (JSONC) config files are accepted. The caller
 * normally validates the result afterwards.
 *
 * @param path  Path to convert.json.
 * @return Parsed ConvertConfig.
 * @throws std::invalid_argument on malformed JSON; std::runtime_error on unreadable file.
 */
ConvertConfig parseConvertConfigFile(const std::string &path);

/**
 * @brief Validate every camera against the actual sensor resolution of *that* camera.
 *
 * Firmware startup check (design doc §8.6): a device may mix cameras with different
 * sensor resolutions, so the actual resolutions are handed in per camera id — exactly
 * like @ref ReadoutStateConfig. Each camera's declared sensor_size must equal its own
 * actual resolution; a camera with no entry, or an entry naming an unknown camera, is
 * also reported (error code 60).
 *
 * @param config             Parsed ConvertConfig.
 * @param actualSensorSizes  Actual resolution of every camera, keyed by camera id.
 * @return ValidateResult with valid=true if every camera matches its own sensor.
 */
ValidateResult validateConfigSensor(const ConvertConfig &config, const SensorSizeConfig &actualSensorSizes);

/**
 * @brief Validate the config against a single sensor resolution (all cameras equal).
 *
 * Convenience overload for the homogeneous case: it is equivalent to handing in
 * @p actualSensorSize for every camera of @p config.
 *
 * @param config            Parsed ConvertConfig.
 * @param actualSensorSize  Actual sensor resolution [width, height], shared by all cameras.
 * @return ValidateResult with valid=true if all cameras match the actual sensor.
 */
ValidateResult validateConfigSensor(const ConvertConfig &config, const Size &actualSensorSize);

/**
 * @brief Validate that the calibration image fits the sensor as a centred window.
 *
 * The calibration (params_cam.yaml) may be performed either at the sensor's full
 * resolution or on a **centred sub-window** of it with no scaling (design doc §4.10) —
 * e.g. the early TN2608-A-250201157 units were calibrated at 1600x1200 on a 1920x1200
 * sensor. Every camera's image_width/image_height must therefore satisfy
 * `calibration ≤ sensor` with an **even** per-axis difference, so that the centring
 * offset used by @ref anchorCalibrationToSensor is an integer. A calibration window
 * larger than the sensor, or one whose difference is odd, is reported with error code 61.
 *
 * @param calib       Parsed calibration data (params_cam.yaml).
 * @param sensorSize  Sensor resolution [width, height] (config or actual sensor).
 * @return ValidateResult with valid=true if every camera's image size is admissible.
 */
ValidateResult validateCalibrationSensor(const CalibrationData &calib, const Size &sensorSize);

/**
 * @brief Validate every camera's calibration window against that camera's own sensor.
 *
 * Per-camera form of the check above, for devices that mix sensor resolutions: a
 * camera with no entry in @p actualSensorSizes cannot be checked and is reported
 * with error code 61.
 *
 * @param calib              Parsed calibration data (params_cam.yaml).
 * @param actualSensorSizes  Actual resolution of every camera, keyed by camera id.
 * @return ValidateResult with valid=true if every camera's image size is admissible.
 */
ValidateResult validateCalibrationSensor(const CalibrationData &calib, const SensorSizeConfig &actualSensorSizes);

/**
 * @brief Validate that a config belongs to the device that is actually running (multi-product).
 *
 * One firmware image may serve several products, so it embeds several convert_config.json
 * candidates and must pick the right one. The PID alone is usually not enough — several product
 * models share one PID and differ only by the per-camera module model — hence both are checked:
 *
 *   · config.pid must be present and equal @p pid            -> error code 64
 *   · every camera's config.module must equal the reported module -> error code 65
 *     (missing module on either side, a camera with no report, or an unknown camera id)
 *
 * Comparison ignores case, surrounding spaces and a leading "0x", so "0x1201" and "1201" match.
 * Both fields take part in the semantic fingerprint, so the params copy records the identity and
 * a params file generated for another product is rejected by the fingerprint check as well.
 *
 * @param config   Parsed convert_config.json candidate.
 * @param pid      PID reported by the device (e.g. "0x1201").
 * @param modules  Module model of every camera, reported by the device.
 * @return ValidateResult with valid=true when the config belongs to this device.
 */
ValidateResult validateConfigIdentity(const ConvertConfig &config, const std::string &pid,
                                      const ModuleIdConfig &modules);

/**
 * @brief Pick the config that belongs to the device out of the firmware's candidates.
 *
 * Optional convenience for a multi-product firmware: returns the index of the candidate that
 * matches the device identity (see @ref validateConfigIdentity), or -1 when none matches.
 * The library keeps NO list of products — the candidates are supplied by the caller, so a new
 * PID / module model never requires a library change. Callers that load a single config do not
 * need this function at all.
 *
 * @param candidates  All convert_config.json candidates embedded in the firmware.
 * @param pid         PID reported by the device.
 * @param modules     Module model of every camera, reported by the device.
 * @return Index into @p candidates, or -1 when none of them belongs to this device.
 */
int selectConfigByDevice(const std::vector<ConvertConfig> &candidates, const std::string &pid,
                         const ModuleIdConfig &modules);

/**
 * @brief OPTIONAL safety net: validate the read-out orientation declared in the config.
 *
 * A product's raw read-out mirror/flip is known when its convert_config.json is authored, so
 * the config carries the real value and convert_params.json is generated with the correct
 * state — firmware does NOT have to inject or check anything: identifying the right config
 * (PID + module) and matching the sensor resolution is what the startup flow verifies.
 *
 * This function exists only for callers that additionally want to catch a mis-authored
 * `readout_orientation` value: it compares the config's declaration with the state read back
 * from the hardware and never mutates the config. A mismatch is error code 66 — worth failing
 * on, because readout_orientation selects which crop region firmware feeds the pipeline
 * (mapCropToReadoutFrame()): a wrong value costs nothing visually but shifts every intrinsic of
 * that mode ("image looks fine, parameters are all wrong").
 *
 * If a product's state is genuinely only known at runtime, use applyReadoutStates() instead: it
 * writes the actual state into the in-memory config (never the flash file), which changes the
 * fingerprint so the regenerated params record the real state (§4.9 约定 5).
 *
 * @param config  Parsed convert_config.json (or convert_params.json).
 * @param actual  Read-out state read back from the hardware, one entry per camera.
 * @return ValidateResult with valid=true when the config matches the hardware.
 */
ValidateResult validateReadoutOrientation(const ConvertConfig &config, const ReadoutStateConfig &actual);

}  // namespace camadapt

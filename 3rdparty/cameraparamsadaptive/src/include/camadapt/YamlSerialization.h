/**
 * @file YamlSerialization.h
 * @brief YAML parsing and serialization for params_cam.yaml / params_imu.yaml.
 */

#pragma once

#include "ImageTransform.h"
#include "Types.h"

#include <string>

namespace camadapt {

/**
 * @brief Parse params_cam.yaml content into a CalibrationData structure.
 *
 * Uses a minimal line-by-line state machine parser. Only the camera section is
 * captured; calibration_info / environment blocks are recognized and skipped
 * (design doc §5.1).
 *
 * @param yamlContent  Raw YAML string content of params_cam.yaml.
 * @return Parsed CalibrationData.
 * @throws std::runtime_error on parse failure.
 */
CalibrationData parseParamsYaml(const std::string &yamlContent);

/**
 * @brief Serialize a single CameraParams to YAML string.
 *
 * @param params  Camera parameters to serialize.
 * @return YAML formatted string.
 */
std::string toYaml(const CameraParams &params);

/**
 * @brief Serialize complete CalibrationData to YAML string.
 *
 * The output starts directly from the camera section; no calibration_info or
 * environment blocks are emitted (design doc §5.1).
 *
 * @param data  Calibration data to serialize.
 * @return YAML formatted string.
 */
std::string toYaml(const CalibrationData &data);

/**
 * @brief Serialize and save CameraParams to a YAML file (host-side tooling).
 *
 * Equivalent to writeTextFile(path, toYaml(params)); the output starts directly
 * from the camera section (no calibration_info / environment, design doc §7.5).
 * Used to generate the new adaptive-params YAML file (design doc §8.7 step 4).
 *
 * @param path    File path to write (truncated if it exists).
 * @param params  Camera parameters to serialize.
 * @return true on success, false on failure.
 */
bool saveYamlFile(const std::string &path, const CameraParams &params);

/**
 * @brief Serialize and save complete CalibrationData to a YAML file.
 *
 * @param path  File path to write (truncated if it exists).
 * @param data  Calibration data to serialize.
 * @return true on success, false on failure.
 */
bool saveYamlFile(const std::string &path, const CalibrationData &data);

/**
 * @brief Serialize and save CameraImuParams to a YAML file (host-side tooling).
 *
 * Equivalent to writeTextFile(path, toYaml(imu)); the output carries the
 * re-output params_imu.yaml (cam0 + imu0 passthrough + gravity, design doc §5.2).
 *
 * @param path  File path to write (truncated if it exists).
 * @param imu   Camera+IMU parameters to serialize.
 * @return true on success, false on failure.
 */
bool saveYamlFile(const std::string &path, const CameraImuParams &imu);

/**
 * @brief Parse params_imu.yaml content into a CameraImuParams structure.
 *
 * The cam0 block is modeled; the imu0 block is captured verbatim as
 * passthrough text (design doc §5.2).
 *
 * @param yamlContent  Raw YAML string content of params_imu.yaml.
 * @return Parsed CameraImuParams.
 * @throws std::runtime_error on parse failure.
 */
CameraImuParams parseImuYaml(const std::string &yamlContent);

/**
 * @brief Serialize CameraImuParams to YAML string.
 *
 * @param imu  Camera+IMU parameters to serialize.
 * @return YAML formatted string.
 */
std::string toYaml(const CameraImuParams &imu);

/**
 * @brief Parse the IMU's own intrinsics out of a params_imu.yaml content string.
 *
 * Purely additive: @ref CameraImuParams keeps `imu0` as verbatim passthrough text,
 * the file format / `format_version` / every fingerprint are untouched, and this
 * function only reads (design doc §5.2). A firmware therefore does not have to be
 * rebuilt to make the data available to the SDK.
 *
 * Mapping (design doc §5.2):
 * | target | source key in params_imu.yaml |
 * |---|---|
 * | `accel.noise_density` / `random_walk` / `reference_temp` | `imu0.accelerometer.*` |
 * | `accel.bias[3]` | `imu0.AccBias` |
 * | `accel.gravity[3]` | **top-level** `gravity` |
 * | `accel.scale_misalignment[9]` | `imu0.M_acc`, flattened row-major, used as-is (no transpose, no inverse) |
 * | `accel.temp_slope[9]` | `imu0.accelerometer.temp_slope`; **no source key in the current files -> all zeros** |
 * | `gyro.noise_density` / `random_walk` / `reference_temp` | `imu0.gyroscope.*` |
 * | `gyro.bias[3]` | `imu0.GyrBias` |
 * | `gyro.scale_misalignment[9]` | `M_gyr * C_gyro_i` (matrix product, that order), flattened row-major |
 * | `gyro.temp_slope[9]` | `imu0.gyroscope.temp_slope`; **no source key in the current files -> all zeros** |
 * | `mag` / `has_mag` | no magnetometer in the file yet -> all zeros / false |
 *
 * `M_acc`, `M_gyr`, `C_gyro_i` and `temp_slope` each accept **both** spellings of a 3x3:
 * three `- [ a, b, c ]` rows, or one inline 9-number sequence
 * `key: [ a, b, c, d, e, f, g, h, i ]` (which may also wrap across lines).
 *
 * Correction models: `AccCalib = M_acc * AccRaw - AccBias` and
 * `GyrCalib = M_gyr * (C_gyro_i * GyrRaw) - GyroBias`. `accel.gravity` is in the
 * **unified world frame (calibration-board plane 0), m/s^2 - NOT the IMU frame**; an
 * `imu0.gravity` is therefore **not** used (only the top-level `gravity` is).
 *
 * Missing keys are not an error: every `scale_misalignment` defaults to the identity
 * matrix (a zero matrix would nullify the signal), every `temp_slope` to zeros and
 * everything else to 0. The remaining `imu0` keys (`imu_model`, `update_rate`,
 * `T_imu_body`, `time_offset`, `Ma_gyr`, unknown keys) are ignored here and preserved
 * verbatim by @ref parseImuYaml.
 *
 * A **malformed 3x3** is rejected as a whole - never left partially filled: a row with
 * a number count other than 3, an inline sequence with a number count other than 9 and
 * an inline sequence left unterminated at end of file all throw. An unknown key inside
 * the `accelerometer` / `gyroscope` sub-block does **not** end it (only a dedent or a
 * new known sub-block does), so keys a future file adds do not shadow the recognised
 * keys after them.
 *
 * @param yamlContent  Raw YAML string content of params_imu.yaml.
 * @return Parsed IMU intrinsics (every field always initialized).
 * @throws std::runtime_error on structurally malformed YAML - a malformed numeric value
 *         under a recognized key, an ill-formed 3x3 matrix or an unterminated inline
 *         sequence - always prefixed with `Line N: `, matching @ref parseImuYaml. This
 *         includes malformed tokens inside a sequence wrapped across several lines.
 */
ImuIntrinsics parseImuIntrinsics(const std::string &yamlContent);

/**
 * @brief Re-output params_imu.yaml data after the master camera changes (design doc §5.2).
 *
 * Updates cam0 intrinsics and resolution from the freshly computed cam0 parameters and
 * applies the mode orientation (rotate/mirror/flip only) to T_cam_imu:
 *   T_cam_imu' = O_3d × T_cam_imu
 * The distortion, imu0 block and gravity are passed through unchanged.
 *
 * @param original   Current params_imu.yaml content.
 * @param newCam0   Computed cam0 parameters at the new resolution/orientation.
 * @param orientation Mode transform whose rotate/mirror/flip defines O_3d.
 * @return Updated CameraImuParams.
 */
CameraImuParams reoutputImuParams(const CameraImuParams &original, const CameraParams &newCam0, const ImageTransform &orientation);

}  // namespace camadapt
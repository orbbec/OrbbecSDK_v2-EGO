/**
 * @file Types.h
 * @brief Foundational geometric types, enums, and camera parameter structures.
 */

#pragma once

#include <limits>
#include <string>
#include <vector>

namespace camadapt {

// ============================================================================
// Basic Geometry Types
// ============================================================================

/**
 * @brief 2D integer size (width, height).
 */
struct Size {
    int width;
    int height;
};

/**
 * @brief 2D integer rectangle.
 */
struct Rect {
    int x;
    int y;
    int width;
    int height;
};

/**
 * @brief 2D double vector.
 */
struct Vec2 {
    double x;
    double y;
};

/**
 * @brief 3D double vector.
 */
struct Vec3 {
    double x;
    double y;
    double z;
};

/**
 * @brief 3x3 double matrix, row-major: m[row][col].
 */
struct Mat33 {
    double m[3][3];
};

// ============================================================================
// Rotation Enum
// ============================================================================

/**
 * @brief Rotation angle in counter-clockwise direction.
 *
 * The enumerator values equal the physical angle in degrees, which lets them be
 * read/written from JSON/YAML without any mapping.
 */
enum class Rotation : int {
    DEG_0   = 0,    ///< 0 degrees
    DEG_90  = 90,   ///< 90 degrees clockwise (image frame: x right, y down; design doc §9.3)
    DEG_180 = 180,  ///< 180 degrees
    DEG_270 = 270   ///< 270 degrees clockwise (image frame: x right, y down; design doc §9.3)
};

// ============================================================================
// Camera Parameter Sub-structures
// ============================================================================

/**
 * @brief Camera intrinsics (pinhole model).
 */
struct Intrinsics {
    double fx;  ///< Focal length x (pixels)
    double fy;  ///< Focal length y (pixels)
    double cx;  ///< Principal point x (pixels)
    double cy;  ///< Principal point y (pixels)
};

/**
 * @brief Camera distortion coefficients (Brown-Conrady model).
 */
struct Distortion {
    std::string model;  ///< Distortion model: "brown"
    double      k1;     ///< Radial distortion coefficient 1
    double      k2;     ///< Radial distortion coefficient 2
    double      k3;     ///< Radial distortion coefficient 3
    double      k4;     ///< Radial distortion coefficient 4 (rational)
    double      k5;     ///< Radial distortion coefficient 5 (rational)
    double      k6;     ///< Radial distortion coefficient 6 (rational)
    double      p1;     ///< Tangential distortion coefficient 1
    double      p2;     ///< Tangential distortion coefficient 2
};

/**
 * @brief Camera extrinsics (pose relative to reference camera).
 *
 * X_cam = rotation * X_world + translation
 */
struct Extrinsics {
    Mat33 rotation;     ///< 3x3 rotation matrix
    Vec3  translation;  ///< Translation vector [tx, ty, tz] in mm
};

// ============================================================================
// Complete Camera Parameters
// ============================================================================

/**
 * @brief Complete camera parameters for a single camera at a given resolution.
 */
struct CameraParams {
    CameraParams()
        : camera_id(), camera_name(), image_size(), intrinsics(), distortion(), extrinsics(), vcm_code(0),
          calibration_temperature(std::numeric_limits<double>::quiet_NaN()) {
        memset(reserved, 0, sizeof(reserved));
    }

    std::string  camera_id;       ///< Camera ID (string, e.g. "cam_0")
    std::string  camera_name;     ///< Camera name
    Size         image_size;      ///< Image resolution (width, height)
    Intrinsics   intrinsics;      ///< Camera intrinsics
    Distortion   distortion;      ///< Distortion coefficients
    Extrinsics   extrinsics;      ///< Camera extrinsics
    int          vcm_code;        ///< VCM focus position (0 = infinity)
    double       calibration_temperature;  ///< Calibration temperature (°C) from params_cam.yaml environment.temperature; NaN when the calibration file does not provide one
    int          reserved[16];    ///< Reserved for future extension
};

// ============================================================================
// Validation Result
// ============================================================================

/**
 * @brief Result of a validation operation.
 */
struct ValidateResult {
    ValidateResult() : valid(false), error_code(0), error_message() {}

    bool        valid;          ///< Whether validation passed
    int         error_code;     ///< Error code (0 = success)
    std::string error_message;  ///< Error description
};

// ============================================================================
// Calibration Data (params_cam.yaml)
// ============================================================================

/**
 * @brief Complete calibration data (corresponds to the camera section of
 *        params_cam.yaml).
 *
 * The source YAML may additionally carry calibration_info / environment blocks used
 * for calibration records (see design doc §5.1); those blocks are intentionally
 * not modeled here and are dropped on serialization - the YAML output starts
 * directly from the camera section.
 */
struct CalibrationData {
    CalibrationData() {
        memset(reserved, 0, sizeof(reserved));
    }

    std::vector<CameraParams>  cameras;  ///< Original resolution camera parameters
    int                        reserved[16];  ///< Reserved for future extension
};

// ============================================================================
// params_imu.yaml (camera + IMU)
// ============================================================================

/**
 * @brief Camera+IMU calibration data (corresponds to params_imu.yaml).
 *
 * Only the camera side (cam0 intrinsics, resolution, distortion, T_cam_imu) is
 * modeled and re-output by the SDK when the master camera resolution or orientation
 * changes; the imu0 block is passthrough data that is never transformed.
 *
 * The 4x4 homogeneous transform T_cam_imu is expressed via @ref Extrinsics
 * (rotation 3x3 + translation 3) as required by the design doc §5.2.
 */
struct CameraImuParams {
    CameraImuParams() : timeshift_cam_imu(0.0), gravity() {
        memset(reserved, 0, sizeof(reserved));
    }

    Intrinsics                intrinsics;          ///< cam0 intrinsics
    Size                     resolution;          ///< cam0 resolution [width, height]
    std::string              distortion_model;    ///< Distortion model (e.g. "KB")
    std::vector<double>      distortion_coeffs;  ///< Distortion coefficients (KB: [k1..k4])
    double                  timeshift_cam_imu;   ///< Time shift between camera and IMU
    Extrinsics              T_cam_imu;          ///< Transform from IMU frame to camera frame
    std::vector<std::string> cam0_order;         ///< cam0 field emission order: known-key tokens ("T_cam_imu", "timeshift_cam_imu", "intrinsics", "distortion_model", "distortion_coeffs", "resolution") or raw "key: value" lines for non-modeled fields; preserves the source order on re-output (§5.2)
    std::string              imu0_text;          ///< Raw passthrough imu0 block (never transformed)
    std::vector<double>      gravity;            ///< Gravity vector [gx, gy, gz], in the **unified world frame (calibration-board plane 0)**, m/s^2 - NOT the IMU frame (design doc §5.2)
    int                      reserved[16];       ///< Reserved for future extension
};

// ============================================================================
// IMU Intrinsics (the imu0 block of params_imu.yaml)
// ============================================================================

/**
 * @brief Accelerometer intrinsics.
 *
 * Mirrors the released SDK struct `OBAccelIntrinsic` (orbbec_accelerometer)
 * field-for-field, so the SDK can copy field-by-field (never memcpy the whole
 * struct: the SDK struct may carry extra fields or a different packing).
 *
 * The 9-element arrays are 3x3 matrices in **row-major** order: `m[r * 3 + c]` is
 * row @c r, column @c c.
 *
 * Correction model (design doc §5.2):
 * @code
 *   AccCalib = M_acc * AccRaw - AccBias
 * @endcode
 * @ref scale_misalignment is the file's `M_acc`, flattened row-major and used
 * **as-is**: no transpose and no inverse is applied, because the file already
 * stores the matrix that left-multiplies the raw reading.
 *
 * @note @ref gravity is in the **unified world frame (calibration-board plane 0),
 *       in m/s^2 - NOT the IMU frame** (design doc §5.2). It is the top-level
 *       `gravity` key of params_imu.yaml, copied here for the SDK's convenience.
 */
struct AccelIntrinsics {
    double noise_density;      ///< Noise density
    double random_walk;        ///< Random walk
    double reference_temp;     ///< Reference temperature (degC) the intrinsics were calibrated at
    double bias[3];            ///< Accelerometer bias (file key `AccBias`)
    double gravity[3];         ///< Gravity vector [gx, gy, gz] in the **unified world frame (calibration-board plane 0)**, m/s^2 - NOT the IMU frame
    double scale_misalignment[9];  ///< 3x3 scale/misalignment matrix, row-major (file key `M_acc`, used as-is)
    double temp_slope[9];      ///< 3x3 temperature slope, row-major (file key `imu0.accelerometer.temp_slope`; no source key in the current files -> all zeros)
};

/**
 * @brief Gyroscope intrinsics.
 *
 * Mirrors the released SDK struct `OBGyroIntrinsic` (orbbec_gyroscope)
 * field-for-field - **no `gravity`** (gravity is an accelerometer quantity, and it
 * lives in the unified world frame, not the IMU frame).
 *
 * The 9-element arrays are 3x3 matrices in **row-major** order: `m[r * 3 + c]` is
 * row @c r, column @c c.
 *
 * Correction model (design doc §5.2):
 * @code
 *   GyrCalib = M_gyr * (C_gyro_i * GyrRaw) - GyroBias
 * @endcode
 * @ref scale_misalignment is therefore the flattened row-major **matrix product**
 * `M_gyr * C_gyro_i` (in that order), not simply `M_gyr`.
 */
struct GyroIntrinsics {
    double noise_density;      ///< Noise density
    double random_walk;        ///< Random walk
    double reference_temp;     ///< Reference temperature (degC) the intrinsics were calibrated at
    double bias[3];            ///< Gyroscope bias (file key `GyrBias`)
    double scale_misalignment[9];  ///< 3x3 scale/misalignment matrix, row-major = `M_gyr * C_gyro_i`
    double temp_slope[9];      ///< 3x3 temperature slope, row-major (file key `imu0.gyroscope.temp_slope`; no source key in the current files -> all zeros)
};

/**
 * @brief Magnetometer intrinsics.
 *
 * Mirrors the released SDK struct `orbbec_magnetometer`, whose field set is
 * **different** from the accelerometer/gyroscope ones: no noise density, no random
 * walk, no bias and no gravity; it carries a different misalignment matrix plus the
 * soft-iron matrix, a per-axis scale and the hard-iron offset.
 *
 * The 9-element arrays are 3x3 matrices in **row-major** order: `m[r * 3 + c]` is
 * row @c r, column @c c.
 *
 * This type is the **forward-compatibility slot** for a future magnetometer: no
 * magnetometer section exists in params_imu.yaml yet, so @ref ImuIntrinsics::has_mag
 * is false and every field here stays zero. Adding one later changes **no
 * `format_version`, no fingerprint and no existing struct layout** (the `mag` /
 * `has_mag` slot is pre-reserved), but it **does require a library release with a new
 * parse branch** to be mapped out: until then a `magnetometer` section is simply not
 * modelled and leaves @ref ImuIntrinsics::has_mag false (design doc §5.2).
 */
struct MagIntrinsics {
    double reference_temp;     ///< Reference temperature (degC) the intrinsics were calibrated at
    double temp_slope[9];      ///< 3x3 temperature slope, row-major
    double misalignment[9];    ///< 3x3 misalignment matrix, row-major
    double soft_iron[9];       ///< 3x3 soft-iron matrix, row-major
    double scale[3];           ///< Per-axis scale
    double hard_iron[3];       ///< Hard-iron offset
};

/**
 * @brief The IMU's own intrinsics, parsed out of the `imu0` block of params_imu.yaml.
 *
 * Produced by @ref parseImuIntrinsics(). This is a purely **additive** read-only
 * view: @ref CameraImuParams keeps the `imu0` block as verbatim passthrough text
 * (`imu0_text`), the file format and `format_version` are unchanged, and IMU
 * intrinsics never participate in any fingerprint - so a firmware does not have to
 * be rebuilt to gain this data (design doc §5.2).
 *
 * @ref mag / @ref has_mag are the **set-level forward-compatibility slot** for a
 * future magnetometer section: @ref has_mag distinguishes "this file has no
 * magnetometer" (false, all of @ref mag zero) from "the magnetometer is present but
 * happens to read all zeros" (true).
 */
struct ImuIntrinsics {
    AccelIntrinsics accel;  ///< Accelerometer intrinsics (bias + `M_acc` + top-level gravity)
    GyroIntrinsics  gyro;   ///< Gyroscope intrinsics (bias + `M_gyr * C_gyro_i`)
    MagIntrinsics   mag;    ///< Magnetometer slot - all zeros until a magnetometer section appears in the file
    bool            has_mag;  ///< Whether the magnetometer slot is populated (currently always false)
};

}  // namespace camadapt
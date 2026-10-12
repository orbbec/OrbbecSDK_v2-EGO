/**
 * @file ImageTransform.h
 * @brief Image transform descriptor and mode/configuration structures for convert.json.
 */

#pragma once

#include "Types.h"

#include <string>
#include <vector>

namespace camadapt {

// Forward declaration: the read-out state types below are declared before
// ConvertConfig is defined, but applyReadoutStates() only needs a reference to it.
struct ConvertConfig;

// ============================================================================
// Crop alignment
// ============================================================================

/**
 * @brief Bayer-grid alignment for crop rectangles (design doc §4.5).
 *
 * Colour sensors use a Bayer pattern, so a crop origin or size that is not a
 * multiple of this factor shifts the colour phase (e.g. RGGB -> GRBG) and
 * corrupts the image. Cropping may happen either in the sensor or in the ISP,
 * so the constraint is applied unconditionally; a mono sensor only loses at
 * most one pixel of centring accuracy as a result.
 *
 * Mirroring a crop (re-expressing its origin in the sensor's read-out frame)
 * only preserves this alignment when (sensor_size − crop_size) is a multiple of
 * the factor too, so that rule is enforced as well (validateConvertJson, §3.6).
 */
constexpr int kCropAlignment = 2;

// ============================================================================
// Read-out orientation
// ============================================================================

/**
 * @brief Mirror/flip firmware applies to the sensor read-out so the image is upright.
 *
 * Structural installation (e.g. routing the flex cable a particular way) can leave a
 * module mounted upside down; firmware then mirrors/flips the sensor read-out so the
 * picture comes out upright. Calibration is performed on that *corrected* image, so
 * K_calib already contains this correction and the SDK must NOT apply it a second time
 * (design doc §4.9 约定 3).
 *
 * How firmware obtains the value (structural table, sensor-register read-back, ...) and
 * how it programs the registers is firmware's business; the library only consumes the
 * value. The field is part of the semantic fingerprint so that recording it regenerates
 * convert_params.json and the params copy ends up holding the ACTUAL state (§4.9 约定 5).
 */
struct ReadoutOrientation {
    ReadoutOrientation() : mirror(false), flip(false) {}
    ReadoutOrientation(bool mirrorValue, bool flipValue) : mirror(mirrorValue), flip(flipValue) {}

    bool mirror;  ///< Sensor read-out is mirrored horizontally (x → −x)
    bool flip;    ///< Sensor read-out is flipped vertically (y → −y)
};

// ============================================================================
// Read-out state handed in by firmware
// ============================================================================

/**
 * @brief One camera's actual read-out state, as configured by firmware.
 */
struct CameraReadoutState {
    std::string        camera_id;    ///< Must match CameraConfig::id
    ReadoutOrientation orientation;  ///< Actual mirror/flip of that camera
};

/**
 * @brief The complete set of read-out states firmware configured for a device.
 *
 * Firmware fills one entry per camera and hands the whole set to
 * applyReadoutStates(). Matching is by camera id, so entry order does not matter and
 * calling it repeatedly is safe.
 *
 * Firmware-side usage:
 * @code
 *   camadapt::ReadoutStateConfig actual;
 *   actual.set("cam_0", false, false);   // this module is mounted upright
 *   actual.set("cam_1", true,  true);    // this one is mounted upside down
 *   camadapt::applyReadoutStates(config, actual);
 * @endcode
 */
struct ReadoutStateConfig {
    std::vector<CameraReadoutState> cameras;  ///< One entry per camera of the device

    /**
     * @brief Set (or replace) one camera's actual read-out state.
     * @param cameraId  Camera id as it appears in convert_config.json.
     * @param mirror    Actual horizontal mirror.
     * @param flip      Actual vertical flip.
     */
    void set(const std::string &cameraId, bool mirror, bool flip);
};

/**
 * @brief Write the firmware-configured read-out states into a config (in memory only).
 *
 * Firmware calls this once at startup, after configuring the sensor read-out and BEFORE
 * the fingerprint is computed, so everything downstream (fingerprint, params
 * regeneration, crop mapping) sees the actual state.
 *
 * What the library guarantees:
 *   · every camera of @p config must appear in @p actual — a missing camera throws, so a
 *     module cannot be silently left unconfigured;
 *   · an entry naming an unknown camera throws as well, as does a duplicated id;
 *   · assignment is absolute (never a toggle), so repeated calls are harmless;
 *   · only the in-memory config is modified — convert_config.json on disk is never
 *     rewritten (design doc §4.9 约定 5).
 *
 * @param config  Parsed config, updated in place.
 * @param actual  States configured by firmware.
 * @return Number of cameras whose value actually changed (0 when already in sync).
 * @throws std::invalid_argument when a camera of @p config is missing from @p actual,
 *         when an entry names an unknown camera, or when an id appears twice.
 */
size_t applyReadoutStates(ConvertConfig &config, const ReadoutStateConfig &actual);

// ============================================================================
// Sensor resolution handed in by firmware
// ============================================================================

/**
 * @brief One camera's actual sensor resolution, as enumerated by firmware.
 */
struct CameraSensorSize {
    std::string camera_id;    ///< Must match CameraConfig::id
    Size        sensor_size;  ///< Actual native resolution of that camera's sensor
};

/**
 * @brief The actual sensor resolution of every camera of a device.
 *
 * A module may mix cameras with **different** sensor resolutions (e.g. a 1920x1200
 * wide module next to a 1600x1300 one), so the check cannot be a single device-wide
 * size — it is per camera, matched by id, exactly like @ref ReadoutStateConfig.
 * The declared sensor_size in each camera's config entry must equal its actual
 * resolution; everything downstream (crop clamping, calibration anchoring) is then
 * driven by that camera's own sensor_size.
 *
 * Firmware-side usage:
 * @code
 *   camadapt::SensorSizeConfig actualSensors;
 *   actualSensors.set("cam_0", 1920, 1200);   // read back from this sensor
 *   actualSensors.set("cam_1", 1600, 1300);   // ...and from this one
 *   ValidateResult vr = camadapt::validateConfigSensor(config, actualSensors);
 *   if(!vr.valid) { ... }                     // config/hardware mismatch -> refuse to run
 * @endcode
 */
struct SensorSizeConfig {
    std::vector<CameraSensorSize> cameras;  ///< One entry per camera of the device

    /**
     * @brief Set (or replace) one camera's actual sensor resolution.
     * @param cameraId  Camera id as it appears in convert_config.json.
     * @param width     Actual sensor width in pixels.
     * @param height    Actual sensor height in pixels.
     */
    void set(const std::string &cameraId, int width, int height);
};

// ============================================================================
// Module model handed in by firmware (multi-product identification)
// ============================================================================

/**
 * @brief One camera's actual module model (e.g. "rgb2610", "irm2405").
 */
struct CameraModuleId {
    std::string camera_id;  ///< Must match CameraConfig::id
    std::string module;     ///< Module model of that camera's module, lowercase
};

/**
 * @brief The module model of every camera of a device, as read from hardware.
 *
 * A single firmware image may serve several products that share a PID, so the PID alone is
 * not enough to pick the right convert_config.json: the per-camera module model is what
 * distinguishes them (e.g. two products on the same sensor but with different modules /
 * lenses / FOV). Firmware fills one entry per camera and hands the set to
 * validateConfigIdentity() — the same per-camera hand-in pattern as ReadoutStateConfig and
 * SensorSizeConfig.
 */
struct ModuleIdConfig {
    std::vector<CameraModuleId> cameras;  ///< One entry per camera of the device

    /**
     * @brief Set (or replace) one camera's module model.
     * @param cameraId  Camera id as it appears in convert_config.json.
     * @param module    Module model, lowercase (e.g. "rgb2615").
     */
    void set(const std::string &cameraId, const std::string &module);
};

// ============================================================================
// Image Transform Descriptor
// ============================================================================

/**
 * @brief Describes an image transform pipeline: Crop → Scale → Rotate → Mirror → Flip.
 */
struct ImageTransform {
    ImageTransform() : rotate(Rotation::DEG_0), mirror(false), flip(false), skipping(0), binning(0) {
        // crop/scale had no initializer, so a default-constructed CameraMode carried
        // indeterminate values (MSVC debug fill: crop = 0xCDCDCDCD, scale ≈ -6.3e66) and
        // anything that consumed a mode before filling the transform read garbage.
        // The parser's own defaults for a missing crop_x/crop_y/crop_width/crop_height are
        // 0 (validateConvertJson reports error 37/38 for that) and scale is only ever
        // meaningful as a positive factor with 1.0 = "no magnification" (isUnityScale), so
        // initialise to exactly those semantics: an all-zero (undefined) crop and unity scale.
        // Members are NOT reordered and reserved[] is still zeroed, so the documented
        // append-before-reserved ABI rule (§7.7) is untouched.
        crop.x      = 0;
        crop.y      = 0;
        crop.width  = 0;
        crop.height = 0;
        scale.x     = 1.0;
        scale.y     = 1.0;
        memset(reserved, 0, sizeof(reserved));
    }

    Rect     crop;             ///< Crop region in original image coordinates
    Vec2     scale;            ///< Scale factors (x, y)
    Rotation rotate;           ///< Rotation angle
    bool     mirror;           ///< Horizontal mirror (x → -x)
    bool     flip;             ///< Vertical flip (y → -y)
    int      skipping;         ///< Pixel skipping factor (0 = disabled, >1 = enabled)
    int      binning;          ///< Pixel binning factor (0 = disabled, >1 = enabled)
    int      reserved[16];     ///< Reserved for future extension
};

// ============================================================================
// Mode and Configuration (convert.json)
// ============================================================================

/**
 * @brief A single resolution mode configuration.
 *
 * lock controls adaptivity (design doc §3.3): lock=true keeps the configured
 * values as-is; lock=false lets the parameter tool recompute the crop from the
 * calibration principal point (base modes) or inherit it from a matching base mode
 * (derived modes). Defaults to true.
 */
struct CameraMode {
    CameraMode() : lock(true) {
        memset(reserved, 0, sizeof(reserved));
    }

    std::string    name;           ///< Mode name (unique within a camera)
    std::string    description;    ///< Mode description
    Size           output_size;     ///< Output image resolution (width, height)
    ImageTransform transform;      ///< Image transform to achieve this mode
    bool           lock;           ///< Whether the mode participates in adaptation (default true)
    int            reserved[16];   ///< Reserved for future extension
};

/**
 * @brief Configuration for a single camera, containing all its modes.
 *
 * temp_coeff is the temperature compensation coefficient (px/°C) applied to the
 * focal length at runtime: F' = F + (runtimeTempC - calibTempC) * temp_coeff.
 * 0.0 disables temperature compensation (default, backward compatible).
 */
struct CameraConfig {
    CameraConfig() : id(), temp_coeff(0.0) {
        memset(reserved, 0, sizeof(reserved));
    }

    // Field order is a wire/ABI contract: new fields are appended immediately before
    // `reserved`, never inserted in the middle (design doc §7.7).
    std::string              id;            ///< Camera ID (string, e.g. "cam_0")
    std::string              name;          ///< Camera name
    Size                     sensor_size;    ///< Sensor native resolution
    double                   temp_coeff;    ///< Temperature compensation coefficient (px/°C); 0 = disabled
    std::vector<CameraMode>  modes;         ///< Supported resolution modes
    ReadoutOrientation       readout_orientation;  ///< Read-out mirror/flip (§4.9); filled by firmware
    std::string              module;        ///< Module model, lowercase (e.g. "rgb2610"); multi-product id
    int                      reserved[16];  ///< Reserved for future extension
};

/**
 * @brief Complete convert.json configuration (all cameras).
 *
 * The same structure backs both firmware-zone files (design doc §3.6):
 *   - convert_config.json : file_type = "config"  (manually maintained by the
 *     algorithm team; fingerprint/body_fingerprint always empty)
 *   - convert_params.json : file_type = "params"  (written by generateParamsJson();
 *     fingerprint holds the hash of the config's semantic fields, body_fingerprint the
 *     hash of THIS file's own semantic fields)
 */
struct ConvertConfig {
    ConvertConfig() : format_version(), file_type(), fingerprint(), description() {
        memset(reserved, 0, sizeof(reserved));
    }

    // Field order is a wire/ABI contract: fields are only ever APPENDED here, immediately
    // before `reserved` (design doc §7.7). Do not insert a new member in the middle.
    std::string                format_version;    ///< Format version string
    std::string                file_type;         ///< File type: "config" or "params"
    std::string                fingerprint;       ///< Hash of the SOURCE config's semantic fields (params only)
    std::string                description;       ///< File description
    std::vector<CameraConfig>  cameras;           ///< All camera configurations
    std::string                body_fingerprint;  ///< Hash of THIS file's own semantic fields (params only, §3.6 check b2)
    std::string                pid;               ///< Product PID, e.g. "0x1201"; selects this config (multi-product firmware)
    int                        reserved[16];      ///< Reserved for future extension
};

// ============================================================================
// Format version support window
// ============================================================================

/// Format version this library writes (generateConvertJson / generateParamsJson).
constexpr const char *kFormatVersionCurrent = "2.5";

/// Oldest format version this library accepts. Below it the file predates the
/// readout_orientation contract (§4.9) and the centred-sub-window calibration rule (§4.10);
/// above kFormatVersionCurrent it was written by a newer tool with rules this build does not
/// know (unknown fields are ignored silently, so loading it could produce wrong parameters).
/// Both directions are rejected with error code 63 by validateConvertJson().
constexpr const char *kFormatVersionMin = "2.2";

// ============================================================================
// Crop ↔ read-out frame conversion
// ============================================================================

/**
 * @brief Re-express a crop region's origin in the sensor read-out frame.
 *
 * convert_params.json stores crop origins in the *calibration* frame: the image
 * the calibration saw, which the module's read-out mirror/flip had already made
 * upright (design doc §4.9 约定 1). Firmware crops the raw read-out instead, so
 * when it applies mirror/flip it must move the origin accordingly:
 *
 *   mirror + flip (180°):  x' = W − x − cw,  y' = H − y − ch
 *   mirror only:           x' = W − x − cw,  y' = y
 *   flip only:             x' = x,           y' = H − y − ch
 *
 * Only the origin moves — crop_width/crop_height are unchanged, so the same
 * physical region keeps its size. The result is aligned back onto the Bayer grid
 * (kCropAlignment) so mirroring can never hand firmware an off-grid origin;
 * validateConvertJson() additionally requires (sensor − crop) to be aligned, so
 * this floor is normally a no-op. The mapping is its own inverse only for origins
 * already aligned to kCropAlignment: an off-grid origin is floored onto the grid
 * and does not come back to itself (e.g. 161 → 158 → 162), so for such input it is
 * a floor, not an exact inverse.
 *
 * @param crop        Crop region as stored in convert_params.json (calibration frame).
 * @param sensorSize  Sensor native resolution.
 * @param readout     Read-out mirror/flip actually applied by firmware.
 * @return Crop region whose origin is expressed in the read-out frame.
 */
Rect mapCropToReadoutFrame(const Rect &crop, const Size &sensorSize, const ReadoutOrientation &readout);

}  // namespace camadapt
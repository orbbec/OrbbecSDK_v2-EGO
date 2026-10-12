/**
 * @file ConvertJsonValidate.cpp
 * @brief Implementation of convert.json validation.
 */

#include "camadapt/ConvertJsonValidate.h"

#include "camadapt/ConvertJsonGen.h"  // computeFingerprint() for check b2 (§3.6)

#include "nlohmann/json.hpp"

#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace camadapt {

// ============================================================================
// Private: JSON parsing helpers
// ============================================================================

namespace {

/**
 * @brief Parse a "major.minor" format version (both parts must be present and numeric).
 */
bool parseMajorMinor(const std::string &version, int &major, int &minor) {
    const size_t dot = version.find('.');
    if(dot == std::string::npos || dot == 0 || dot + 1 >= version.size()) {
        return false;
    }
    try {
        size_t usedMajor = 0;
        size_t usedMinor = 0;
        major            = std::stoi(version.substr(0, dot), &usedMajor);
        minor            = std::stoi(version.substr(dot + 1), &usedMinor);
        if(usedMajor != dot || usedMinor != version.size() - dot - 1) {
            return false;  // trailing garbage such as "2.4-beta"
        }
    } catch(const std::exception &) {
        return false;
    }
    return true;
}

/**
 * @brief Parse a ConvertConfig from a nlohmann::json object (flat layout, §3.1).
 */
ConvertConfig fromJson(const nlohmann::json &j) {
    ConvertConfig config;

    config.format_version   = j.value("format_version", "");
    config.file_type        = j.value("file_type", "");
    config.fingerprint      = j.value("fingerprint", "");
    config.body_fingerprint = j.value("body_fingerprint", "");  // params files only (§3.6 check b2)
    config.pid              = j.value("pid", "");               // product PID (multi-product firmware)
    config.description      = j.value("description", "");

    if(j.contains("cameras") && j["cameras"].is_array()) {
        for(const auto &camJson : j["cameras"]) {
            CameraConfig cam;
            cam.id   = camJson.value("id", std::string());
            cam.name = camJson.value("name", "");
            cam.module = camJson.value("module", "");  // module model, lowercase (identity)
            cam.sensor_size.width  = camJson.value("sensor_width", 0);
            cam.sensor_size.height = camJson.value("sensor_height", 0);
            cam.temp_coeff         = camJson.value("temp_coeff", 0.0);  // 0 = temperature compensation disabled (§3.7)

            // Read-out mirror/flip (§4.9). Parsed leniently here; whether the
            // block was actually present is checked against the raw JSON by
            // validateReadoutOrientationPresence(), because a parsed CameraConfig
            // cannot distinguish "absent" from "{mirror:false, flip:false}".
            if(camJson.contains("readout_orientation") && camJson["readout_orientation"].is_object()) {
                const nlohmann::json &ro       = camJson["readout_orientation"];
                cam.readout_orientation.mirror = ro.value("mirror", false);
                cam.readout_orientation.flip   = ro.value("flip", false);
            }

            if(camJson.contains("modes") && camJson["modes"].is_array()) {
                for(const auto &modeJson : camJson["modes"]) {
                    CameraMode mode;
                    mode.name                  = modeJson.value("name", "");
                    mode.description           = modeJson.value("description", "");
                    mode.output_size.width      = modeJson.value("width", 0);
                    mode.output_size.height     = modeJson.value("height", 0);
                    mode.transform.crop.x      = modeJson.value("crop_x", 0);
                    mode.transform.crop.y      = modeJson.value("crop_y", 0);
                    mode.transform.crop.width  = modeJson.value("crop_width", 0);
                    mode.transform.crop.height = modeJson.value("crop_height", 0);
                    mode.transform.scale.x     = modeJson.value("scale_x", 0.0);
                    mode.transform.scale.y     = modeJson.value("scale_y", 0.0);
                    mode.transform.rotate      = static_cast<Rotation>(modeJson.value("rotate", 0));
                    mode.transform.mirror      = modeJson.value("mirror", false);
                    mode.transform.flip        = modeJson.value("flip", false);
                    mode.transform.skipping    = modeJson.value("skipping", 0);
                    mode.transform.binning     = modeJson.value("binning", 0);
                    mode.lock                = modeJson.value("lock", true);  // lock defaults to true (§3.3)

                    cam.modes.push_back(mode);
                }
            }

            config.cameras.push_back(cam);
        }
    }

    return config;
}

/**
 * @brief Compute skip+binning factor for a mode.
 */
double skipBinningFactor(const CameraMode &mode) {
    double factor = 1.0;
    if(mode.transform.skipping > 1) {
        factor *= static_cast<double>(mode.transform.skipping);
    }
    if(mode.transform.binning > 1) {
        factor *= static_cast<double>(mode.transform.binning);
    }
    return factor;
}

/**
 * @brief Check the required per-camera read-out orientation block (design doc §4.9).
 *
 * The read-out mirror/flip is a per-camera, product-structural fact that firmware
 * fills into convert_config.json. It is required so that it cannot be silently
 * forgotten, and its presence must be checked against the raw JSON: a parsed
 * CameraConfig cannot tell "absent" apart from "{mirror:false, flip:false}".
 *
 * @param jsonContent  Raw convert.json content.
 * @return ValidateResult with valid=true when every camera declares the block
 *         with boolean mirror/flip members.
 */
ValidateResult validateReadoutOrientationPresence(const std::string &jsonContent) {
    ValidateResult result;
    result.valid      = true;
    result.error_code = 0;

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(jsonContent, nullptr, true, true);
    } catch(const std::exception &) {
        return result;  // malformed JSON is reported by validateJsonFormat()
    }

    if(!j.contains("cameras") || !j["cameras"].is_array()) {
        return result;  // structural checks live in validateFieldValues()
    }

    for(size_t i = 0; i < j["cameras"].size(); ++i) {
        const nlohmann::json &camJson = j["cameras"][i];
        if(!camJson.contains("readout_orientation") || !camJson["readout_orientation"].is_object()) {
            result.valid      = false;
            result.error_code = 46;
            result.error_message =
                "cameras[" + std::to_string(i) + "]: readout_orientation is required "
                "(object with boolean mirror/flip, design doc §4.9)";
            return result;
        }
        const nlohmann::json &ro = camJson["readout_orientation"];
        if(!ro.contains("mirror") || !ro.contains("flip") || !ro["mirror"].is_boolean() || !ro["flip"].is_boolean()) {
            result.valid      = false;
            result.error_code = 46;
            result.error_message = "cameras[" + std::to_string(i)
                                   + "]: readout_orientation must contain boolean mirror and flip members";
            return result;
        }
    }

    return result;
}

}  // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

ValidateResult validateJsonFormat(const std::string &jsonContent) {
    ValidateResult result;
    try {
        // ignore_comments = true: comment-tolerant, matching parseConvertJson().
        // The parsed value is intentionally discarded: this only checks syntax.
        (void)nlohmann::json::parse(jsonContent, nullptr, true, true);
        result.valid      = true;
        result.error_code = 0;
    } catch(const nlohmann::json::parse_error &e) {
        result.valid         = false;
        result.error_code    = 1;
        result.error_message = std::string("JSON parse error: ") + e.what();
    } catch(const std::exception &e) {
        result.valid         = false;
        result.error_code    = 2;
        result.error_message = std::string("Unexpected error: ") + e.what();
    }
    return result;
}

ConvertConfig parseConvertJson(const std::string &jsonContent) {
    try {
        // ignore_comments = true: accept the algorithm team's annotated (JSONC) configs.
        nlohmann::json j = nlohmann::json::parse(jsonContent, nullptr, true, true);
        return fromJson(j);
    } catch(const std::exception &e) {
        throw std::invalid_argument(std::string("Failed to parse convert.json: ") + e.what());
    }
}

ConvertConfig parseConvertConfigFile(const std::string &path) {
    return parseConvertJson(readTextFile(path));
}

ValidateResult validateFieldValues(const ConvertConfig &config) {
    ValidateResult result;

    // Check format_version (design doc §3.6 / §7.7): the version must be inside the window
    // this build understands. Below the minimum the file predates fields/behaviour the
    // library assumes; above the current version it was written by a newer tool whose rules
    // this build does not know — and since unknown JSON keys are ignored silently, loading
    // such a file could quietly produce wrong parameters instead of failing. Both directions
    // are reported with error code 63 so the caller upgrades or regenerates.
    if(config.format_version.empty()) {
        result.valid         = false;
        result.error_code    = 10;
        result.error_message = "format_version is empty";
        return result;
    }
    {
        int fileMajor = 0, fileMinor = 0;
        int minMajor = 0, minMinor = 0;
        int curMajor = 0, curMinor = 0;
        const bool fileOk = parseMajorMinor(config.format_version, fileMajor, fileMinor);
        parseMajorMinor(kFormatVersionMin, minMajor, minMinor);
        parseMajorMinor(kFormatVersionCurrent, curMajor, curMinor);
        if(!fileOk) {
            result.valid      = false;
            result.error_code = 63;
            result.error_message = "format_version \"" + config.format_version
                                 + "\" is not a major.minor version (supported window: " + kFormatVersionMin + " .. "
                                 + kFormatVersionCurrent + ")";
            return result;
        }
        const bool tooOld = (fileMajor < minMajor) || (fileMajor == minMajor && fileMinor < minMinor);
        const bool tooNew = (fileMajor > curMajor) || (fileMajor == curMajor && fileMinor > curMinor);
        if(tooOld || tooNew) {
            result.valid      = false;
            result.error_code = 63;
            result.error_message =
                "format_version \"" + config.format_version + "\" is outside the supported window " + kFormatVersionMin
                + " .. " + kFormatVersionCurrent + " (" + (tooOld ? "file is too old" : "file was written by a newer tool")
                + ") -> " + (tooOld ? "regenerate convert_params.json with this library" : "upgrade this library");
            return result;
        }
    }

    // Check file_type (design doc §3.6): "config" or "params"
    if(config.file_type != "config" && config.file_type != "params") {
        result.valid         = false;
        result.error_code    = 12;
        result.error_message = "file_type must be \"config\" or \"params\", got \"" + config.file_type + "\"";
        return result;
    }

    // Check cameras array
    if(config.cameras.empty()) {
        result.valid         = false;
        result.error_code    = 11;
        result.error_message = "cameras array is empty";
        return result;
    }

    // Camera ids must be unique: SensorSizeConfig / ModuleIdConfig / ReadoutStateConfig are
    // keyed by camera id, so two cameras sharing one id would be silently collapsed into one
    // entry and the second camera's modes would be checked against the first camera's sensor
    // and module (§3.6).
    std::set<std::string> cameraIds;

    for(size_t camIdx = 0; camIdx < config.cameras.size(); ++camIdx) {
        const auto &cam = config.cameras[camIdx];
        std::string camPrefix =
            "cameras[" + std::to_string(camIdx) + "]" + (cam.name.empty() ? "" : " (name=" + cam.name + ")");

        if(cam.id.empty()) {
            result.valid         = false;
            result.error_code    = 20;
            result.error_message = camPrefix + ": id is empty";
            return result;
        }

        if(!cameraIds.insert(cam.id).second) {
            result.valid         = false;
            result.error_code    = 23;
            result.error_message = "cameras[" + std::to_string(camIdx) + "]: duplicate camera id \"" + cam.id + "\"";
            return result;
        }

        if(cam.name.empty()) {
            result.valid         = false;
            result.error_code    = 21;
            result.error_message = camPrefix + ": name is empty";
            return result;
        }

        if(cam.modes.empty()) {
            result.valid         = false;
            result.error_code    = 22;
            result.error_message = camPrefix + ": modes array is empty";
            return result;
        }

        // Temperature compensation coefficient must be finite (§3.7)
        if(!std::isfinite(cam.temp_coeff)) {
            result.valid         = false;
            result.error_code    = 70;
            result.error_message = camPrefix + ": temp_coeff must be finite, got " + std::to_string(cam.temp_coeff);
            return result;
        }

        // Check mode name uniqueness and output resolution uniqueness (§3.6)
        std::set<std::string> modeNames;
        std::set<std::string> outputSizes;
        for(size_t modeIdx = 0; modeIdx < cam.modes.size(); ++modeIdx) {
            const auto &mode = cam.modes[modeIdx];
            std::string modePrefix = camPrefix + ".modes[" + std::to_string(modeIdx) + "]";

            if(mode.name.empty()) {
                result.valid         = false;
                result.error_code    = 30;
                result.error_message = modePrefix + ": name is empty";
                return result;
            }

            if(!modeNames.insert(mode.name).second) {
                result.valid         = false;
                result.error_code    = 31;
                result.error_message = modePrefix + ": duplicate mode name '" + mode.name + "'";
                return result;
            }

            std::string sizeKey = std::to_string(mode.output_size.width) + "x" + std::to_string(mode.output_size.height);
            if(!outputSizes.insert(sizeKey).second) {
                result.valid         = false;
                result.error_code    = 32;
                result.error_message =
                    modePrefix + ": duplicate output resolution " + sizeKey + " within the same camera";
                return result;
            }

            if(mode.output_size.width <= 0) {
                result.valid         = false;
                result.error_code    = 33;
                result.error_message = modePrefix + ": width must be > 0, got " + std::to_string(mode.output_size.width);
                return result;
            }

            if(mode.output_size.height <= 0) {
                result.valid         = false;
                result.error_code    = 34;
                result.error_message = modePrefix + ": height must be > 0, got " + std::to_string(mode.output_size.height);
                return result;
            }

            if(mode.transform.crop.x < 0) {
                result.valid         = false;
                result.error_code    = 35;
                result.error_message = modePrefix + ": crop_x must be >= 0";
                return result;
            }

            if(mode.transform.crop.y < 0) {
                result.valid         = false;
                result.error_code    = 36;
                result.error_message = modePrefix + ": crop_y must be >= 0";
                return result;
            }

            if(mode.transform.crop.width <= 0) {
                result.valid         = false;
                result.error_code    = 37;
                result.error_message = modePrefix + ": crop_width must be > 0";
                return result;
            }

            if(mode.transform.crop.height <= 0) {
                result.valid         = false;
                result.error_code    = 38;
                result.error_message = modePrefix + ": crop_height must be > 0";
                return result;
            }

            if(mode.transform.scale.x <= 0.0) {
                result.valid         = false;
                result.error_code    = 39;
                result.error_message = modePrefix + ": scale_x must be > 0";
                return result;
            }

            if(mode.transform.scale.y <= 0.0) {
                result.valid         = false;
                result.error_code    = 40;
                result.error_message = modePrefix + ": scale_y must be > 0";
                return result;
            }

            int rotVal = static_cast<int>(mode.transform.rotate);
            if(rotVal != 0 && rotVal != 90 && rotVal != 180 && rotVal != 270) {
                result.valid         = false;
                result.error_code    = 41;
                result.error_message =
                    modePrefix + ": rotate must be one of {0, 90, 180, 270}, got " + std::to_string(rotVal);
                return result;
            }

            // Check crop region does not exceed sensor bounds (if sensor size is specified)
            if(cam.sensor_size.width > 0 && cam.sensor_size.height > 0) {
                // 64-bit arithmetic: an int sum can wrap negative and silently pass this bound
                // check (e.g. crop_x = 2147483646, crop_width = 2 on a 1600-wide sensor).
                // crop_x/crop_y < 0 is rejected earlier, so only the upper side needs care.
                const long long cropRight =
                    static_cast<long long>(mode.transform.crop.x) + static_cast<long long>(mode.transform.crop.width);
                const long long cropBottom =
                    static_cast<long long>(mode.transform.crop.y) + static_cast<long long>(mode.transform.crop.height);
                if(cropRight > static_cast<long long>(cam.sensor_size.width)) {
                    result.valid      = false;
                    result.error_code = 42;
                    result.error_message =
                        modePrefix + ": crop_x + crop_width (" + std::to_string(cropRight)
                        + ") exceeds sensor_width (" + std::to_string(cam.sensor_size.width) + ")";
                    return result;
                }
                if(cropBottom > static_cast<long long>(cam.sensor_size.height)) {
                    result.valid      = false;
                    result.error_code = 43;
                    result.error_message =
                        modePrefix + ": crop_y + crop_height (" + std::to_string(cropBottom)
                        + ") exceeds sensor_height (" + std::to_string(cam.sensor_size.height) + ")";
                    return result;
                }
            }

            // Bayer-grid alignment (design doc §3.6 / §4.5): the crop origin and
            // size must be multiples of kCropAlignment, otherwise a colour
            // sensor's phase is shifted and the resulting image is corrupted.
            const int cropX = mode.transform.crop.x;
            const int cropY = mode.transform.crop.y;
            const int cropW = mode.transform.crop.width;
            const int cropH = mode.transform.crop.height;

            if((cropX % kCropAlignment) != 0 || (cropY % kCropAlignment) != 0 || (cropW % kCropAlignment) != 0
               || (cropH % kCropAlignment) != 0) {
                result.valid      = false;
                result.error_code = 44;
                result.error_message =
                    modePrefix + ": crop (x=" + std::to_string(cropX) + ", y=" + std::to_string(cropY)
                    + ", w=" + std::to_string(cropW) + ", h=" + std::to_string(cropH) + ") must be a multiple of "
                    + std::to_string(kCropAlignment) + " (Bayer alignment, design doc §4.5)";
                return result;
            }

            // Re-expressing a mirrored origin as x' = sensor_w − x − crop_w stays
            // on the grid only when (sensor − crop) is aligned as well.
            if(cam.sensor_size.width > 0 && cam.sensor_size.height > 0
               && (((cam.sensor_size.width - cropW) % kCropAlignment) != 0
                   || ((cam.sensor_size.height - cropH) % kCropAlignment) != 0)) {
                result.valid      = false;
                result.error_code = 45;
                result.error_message =
                    modePrefix + ": (sensor − crop) must be a multiple of " + std::to_string(kCropAlignment)
                    + " (sensor " + std::to_string(cam.sensor_size.width) + "×" + std::to_string(cam.sensor_size.height)
                    + ", crop " + std::to_string(cropW) + "×" + std::to_string(cropH)
                    + "); a mirrored crop origin would land off the Bayer grid";
                return result;
            }
        }
    }

    // Check b2 (design doc §3.6): a params file must be internally consistent — its body
    // must still hash to the `body_fingerprint` it carries. This is what catches a
    // tampered, truncated or half-flashed body: `fingerprint` only proves the file came
    // from the config (and legitimately differs from the body, because lock=false modes
    // were recomputed), so it cannot detect a body edit on its own.
    if(config.file_type == "params") {
        if(config.body_fingerprint.empty()) {
            result.valid      = false;
            result.error_code = 62;
            result.error_message =
                "params file has no body_fingerprint (written by an older tool) -> regenerate convert_params.json";
            return result;
        }
        const std::string bodyHash = computeFingerprint(config);
        if(bodyHash != config.body_fingerprint) {
            result.valid      = false;
            result.error_code = 62;
            result.error_message = "params body does not match body_fingerprint (tampered/corrupted file): expected "
                                 + config.body_fingerprint + ", got " + bodyHash;
            return result;
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

ValidateResult validateConsistency(const ConvertConfig &config) {
    ValidateResult result;

    for(size_t camIdx = 0; camIdx < config.cameras.size(); ++camIdx) {
        const auto &cam = config.cameras[camIdx];
        std::string camPrefix =
            "cameras[" + std::to_string(camIdx) + "]" + (cam.name.empty() ? "" : " (name=" + cam.name + ")");

        for(size_t modeIdx = 0; modeIdx < cam.modes.size(); ++modeIdx) {
            const auto &mode   = cam.modes[modeIdx];
            std::string prefix = camPrefix + ".modes[" + std::to_string(modeIdx) + "]";
            const auto  &t     = mode.transform;

            double sbFactor = skipBinningFactor(mode);
            double expectedW, expectedH;

            int rotVal = static_cast<int>(t.rotate);
            if(rotVal == 0 || rotVal == 180) {
                // rotate 0° or 180°: width from crop_width, height from crop_height
                expectedW = static_cast<double>(t.crop.width) * t.scale.x * sbFactor;
                expectedH = static_cast<double>(t.crop.height) * t.scale.y * sbFactor;
            } else {
                // rotate 90° or 270°: width from crop_height, height from crop_width
                expectedW = static_cast<double>(t.crop.height) * t.scale.y * sbFactor;
                expectedH = static_cast<double>(t.crop.width) * t.scale.x * sbFactor;
            }

            // width = round(crop_width × eff_scale) — allow half-pixel rounding tolerance
            double actualW = static_cast<double>(mode.output_size.width);
            double actualH = static_cast<double>(mode.output_size.height);

            if(std::abs(actualW - expectedW) > 0.5) {
                result.valid      = false;
                result.error_code = 50;
                std::ostringstream oss;
                oss << prefix << ": width " << actualW << " does not match expected " << expectedW
                    << " (crop=" << t.crop.width << "×" << t.crop.height << ", scale=" << t.scale.x << "×" << t.scale.y
                    << ", rotate=" << rotVal << ", sbFactor=" << sbFactor << ")";
                result.error_message = oss.str();
                return result;
            }

            if(std::abs(actualH - expectedH) > 0.5) {
                result.valid      = false;
                result.error_code = 51;
                std::ostringstream oss;
                oss << prefix << ": height " << actualH << " does not match expected " << expectedH
                    << " (crop=" << t.crop.width << "×" << t.crop.height << ", scale=" << t.scale.x << "×" << t.scale.y
                    << ", rotate=" << rotVal << ", sbFactor=" << sbFactor << ")";
                result.error_message = oss.str();
                return result;
            }
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

ValidateResult validateConfigSensor(const ConvertConfig &config, const SensorSizeConfig &actualSensorSizes) {
    ValidateResult result;

    // An entry naming a camera that the config does not know is a firmware bug: it
    // means the two sides disagree about the device's camera set.
    for(const auto &entry : actualSensorSizes.cameras) {
        bool known = false;
        for(const auto &cam : config.cameras) {
            if(cam.id == entry.camera_id) {
                known = true;
                break;
            }
        }
        if(!known) {
            result.valid         = false;
            result.error_code    = 60;
            result.error_message = "actual sensor resolution given for unknown camera '"
                                 + entry.camera_id + "'";
            return result;
        }
    }

    for(const auto &cam : config.cameras) {
        const CameraSensorSize *actual = nullptr;
        for(const auto &entry : actualSensorSizes.cameras) {
            if(entry.camera_id == cam.id) {
                actual = &entry;
                break;
            }
        }
        if(actual == nullptr) {
            result.valid         = false;
            result.error_code    = 60;
            result.error_message = "camera " + cam.id
                                 + ": no actual sensor resolution given (every camera must be reported)";
            return result;
        }

        if(cam.sensor_size.width != actual->sensor_size.width
           || cam.sensor_size.height != actual->sensor_size.height) {
            result.valid         = false;
            result.error_code    = 60;
            result.error_message = "camera " + cam.id + ": declared sensor " + std::to_string(cam.sensor_size.width)
                                + "x" + std::to_string(cam.sensor_size.height) + " differs from actual sensor "
                                + std::to_string(actual->sensor_size.width) + "x"
                                + std::to_string(actual->sensor_size.height) + " (config/hardware mismatch)";
            return result;
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

ValidateResult validateConfigSensor(const ConvertConfig &config, const Size &actualSensorSize) {
    // Homogeneous case: the same resolution for every camera of the device.
    SensorSizeConfig actualSensorSizes;
    for(const auto &cam : config.cameras) {
        actualSensorSizes.set(cam.id, actualSensorSize.width, actualSensorSize.height);
    }
    return validateConfigSensor(config, actualSensorSizes);
}

ValidateResult validateCalibrationSensor(const CalibrationData &calib, const SensorSizeConfig &actualSensorSizes) {
    ValidateResult result;

    for(size_t i = 0; i < calib.cameras.size(); ++i) {
        const auto &cam = calib.cameras[i];

        // Which sensor did THIS camera's calibration run on? Per camera, because a
        // device may mix sensor resolutions.
        const CameraSensorSize *actual = nullptr;
        for(const auto &entry : actualSensorSizes.cameras) {
            if(entry.camera_id == cam.camera_id) {
                actual = &entry;
                break;
            }
        }
        if(actual == nullptr) {
            result.valid         = false;
            result.error_code    = 61;
            result.error_message = "camera " + cam.camera_id
                                 + ": no actual sensor resolution given, cannot check the calibration window";
            return result;
        }
        const Size sensorSize = actual->sensor_size;

        const int dw = sensorSize.width - cam.image_size.width;
        const int dh = sensorSize.height - cam.image_size.height;

        // The calibration may use the full sensor or a CENTRED sub-window of it with no
        // scaling (design doc §4.10) — e.g. early TN2608-A-250201157 units calibrated at
        // 1600x1200 on a 1920x1200 sensor. Reject anything larger, and reject a
        // sub-window whose centring offset would not be an integer.
        if(dw < 0 || dh < 0) {
            result.valid         = false;
            result.error_code    = 61;
            result.error_message = "camera " + cam.camera_id + ": calibration image "
                                 + std::to_string(cam.image_size.width) + "x"
                                 + std::to_string(cam.image_size.height) + " is larger than the sensor "
                                 + std::to_string(sensorSize.width) + "x" + std::to_string(sensorSize.height);
            return result;
        }
        if(dw % 2 != 0 || dh % 2 != 0) {
            result.valid      = false;
            result.error_code = 61;
            result.error_message =
                "camera " + cam.camera_id + ": sensor minus calibration resolution must be even on both axes "
                "(the calibration window is assumed to be centred), got " + std::to_string(dw) + "x"
                + std::to_string(dh);
            return result;
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

ValidateResult validateCalibrationSensor(const CalibrationData &calib, const Size &sensorSize) {
    // Homogeneous case: every camera's calibration ran on a sensor of this size.
    SensorSizeConfig actualSensorSizes;
    for(const auto &cam : calib.cameras) {
        actualSensorSizes.set(cam.camera_id, sensorSize.width, sensorSize.height);
    }
    return validateCalibrationSensor(calib, actualSensorSizes);
}

namespace {

/**
 * @brief Canonical form of a PID / module string for comparison: lowercase, no spaces,
 *        no leading "0x".
 *
 * Accepts the forms a hardware or product table may use ("0x1201", "0X1201", "1201",
 * " 0x1201 "), so a formatting difference never looks like a different product.
 */
std::string normalizeIdentity(const std::string &value) {
    std::string out;
    for(char c : value) {
        if(c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if(out.size() > 2 && out[0] == '0' && out[1] == 'x') {
        out = out.substr(2);
    }
    return out;
}

}  // namespace

ValidateResult validateConfigIdentity(const ConvertConfig &config, const std::string &pid,
                                     const ModuleIdConfig &modules) {
    ValidateResult result;

    // ── PID ──
    if(config.pid.empty()) {
        result.valid         = false;
        result.error_code    = 64;
        result.error_message = "convert_config.json declares no pid, so it cannot be selected by product (device "
                               "reported pid '" + pid + "')";
        return result;
    }
    if(normalizeIdentity(config.pid) != normalizeIdentity(pid)) {
        result.valid         = false;
        result.error_code    = 64;
        result.error_message = "pid mismatch: config declares '" + config.pid + "', device reports '" + pid + "'";
        return result;
    }

    // ── Per-camera module model ──
    // An entry naming a camera the config does not know means the two sides disagree about the
    // device's camera set.
    for(const auto &entry : modules.cameras) {
        bool known = false;
        for(const auto &cam : config.cameras) {
            if(cam.id == entry.camera_id) {
                known = true;
                break;
            }
        }
        if(!known) {
            result.valid         = false;
            result.error_code    = 65;
            result.error_message = "module reported for unknown camera '" + entry.camera_id + "'";
            return result;
        }
    }

    for(const auto &cam : config.cameras) {
        const CameraModuleId *actual = nullptr;
        for(const auto &entry : modules.cameras) {
            if(entry.camera_id == cam.id) {
                actual = &entry;
                break;
            }
        }
        if(actual == nullptr) {
            result.valid         = false;
            result.error_code    = 65;
            result.error_message = "camera " + cam.id + ": no module reported (every camera must be reported)";
            return result;
        }
        if(cam.module.empty()) {
            result.valid         = false;
            result.error_code    = 65;
            result.error_message = "camera " + cam.id + ": config declares no module, cannot match the hardware";
            return result;
        }
        if(normalizeIdentity(cam.module) != normalizeIdentity(actual->module)) {
            result.valid         = false;
            result.error_code    = 65;
            result.error_message = "camera " + cam.id + ": module mismatch: config declares '" + cam.module
                                 + "', device reports '" + actual->module + "'";
            return result;
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

int selectConfigByDevice(const std::vector<ConvertConfig> &candidates, const std::string &pid,
                         const ModuleIdConfig &modules) {
    for(size_t i = 0; i < candidates.size(); ++i) {
        if(validateConfigIdentity(candidates[i], pid, modules).valid) {
            return static_cast<int>(i);
        }
    }
    return -1;  // no candidate belongs to this device
}

ValidateResult validateReadoutOrientation(const ConvertConfig &config, const ReadoutStateConfig &actual) {
    ValidateResult result;

    // An entry naming a camera the config does not know means the two sides disagree about the
    // device's camera set.
    for(const auto &entry : actual.cameras) {
        bool known = false;
        for(const auto &cam : config.cameras) {
            if(cam.id == entry.camera_id) {
                known = true;
                break;
            }
        }
        if(!known) {
            result.valid         = false;
            result.error_code    = 66;
            result.error_message = "read-out state reported for unknown camera '" + entry.camera_id + "'";
            return result;
        }
    }

    for(const auto &cam : config.cameras) {
        const CameraReadoutState *hw = nullptr;
        for(const auto &entry : actual.cameras) {
            if(entry.camera_id == cam.id) {
                hw = &entry;
                break;
            }
        }
        if(hw == nullptr) {
            result.valid         = false;
            result.error_code    = 66;
            result.error_message = "camera " + cam.id + ": no read-out state reported (every camera must be reported)";
            return result;
        }
        if(cam.readout_orientation.mirror != hw->orientation.mirror
           || cam.readout_orientation.flip != hw->orientation.flip) {
            result.valid      = false;
            result.error_code = 66;
            result.error_message =
                "camera " + cam.id + ": read-out orientation mismatch: config declares {mirror="
                + (cam.readout_orientation.mirror ? "true" : "false") + ", flip="
                + (cam.readout_orientation.flip ? "true" : "false") + "}, hardware reports {mirror="
                + (hw->orientation.mirror ? "true" : "false") + ", flip=" + (hw->orientation.flip ? "true" : "false")
                + "} -> the config's crop mapping would be wrong for every intrinsic of this mode";
            return result;
        }
    }

    result.valid      = true;
    result.error_code = 0;
    return result;
}

ValidateResult validateConvertJson(const std::string &jsonContent) {
    // Step 1: Validate JSON format
    ValidateResult formatResult = validateJsonFormat(jsonContent);
    if(!formatResult.valid) {
        return formatResult;
    }

    // Step 2: Parse into struct
    ConvertConfig config;
    try {
        config = parseConvertJson(jsonContent);
    } catch(const std::exception &e) {
        ValidateResult result;
        result.valid         = false;
        result.error_code    = 3;
        result.error_message = std::string("Failed to convert JSON to struct: ") + e.what();
        return result;
    }

    // Step 3: Validate field values
    ValidateResult fieldResult = validateFieldValues(config);
    if(!fieldResult.valid) {
        return fieldResult;
    }

    // Step 4: Validate consistency
    ValidateResult consistencyResult = validateConsistency(config);
    if(!consistencyResult.valid) {
        return consistencyResult;
    }

    // Step 5: Required per-camera read-out orientation (design doc §4.9). Checked
    // last and against the raw JSON, because a parsed CameraConfig cannot express
    // "absent"; keeping it last preserves the pre-existing error codes.
    return validateReadoutOrientationPresence(jsonContent);
}

}  // namespace camadapt

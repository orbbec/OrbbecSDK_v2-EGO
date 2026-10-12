/**
 * @file ConvertJsonGen.cpp
 * @brief Implementation of convert.json generation and semantic fingerprinting.
 *
 * convert_config.json (file_type "config") is produced for the algorithm team by
 * generateConvertJson()/appendMode(); convert_params.json (file_type "params") is
 * produced at calibration time (and by firmware fallback) via generateParamsJson().
 */

#include "camadapt/ConvertJsonGen.h"

#include "camadapt/ParamTransform.h"
#include "nlohmann/json.hpp"

#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace camadapt {

// ============================================================================
// Private: JSON serialization helpers
// ============================================================================

namespace {

/**
 * @brief Semantic test: is a scale factor "unity" (no magnification)?
 *
 * scale is stored as double, but its source values are config decimals that
 * only need single-precision resolution. Compare with the float epsilon so a
 * near-1.0 value (e.g. 0.9999999) still counts as no magnification while a
 * genuinely different scale (e.g. 0.5) does not (design doc §4.3).
 */
bool isUnityScale(double v) {
    return std::fabs(v - 1.0) <= std::numeric_limits<float>::epsilon();
}

/**
 * @brief Round v to the nearest multiple of kCropAlignment (design doc §4.5).
 *
 * Crop origins must sit on the Bayer grid, otherwise a colour sensor's phase is
 * shifted and the picture is corrupted; so the generator produces aligned
 * origins rather than merely truncated ones.
 */
int alignRound(double v) {
    const double step = static_cast<double>(kCropAlignment);
    return static_cast<int>(std::llround(v / step)) * kCropAlignment;
}

/**
 * @brief Largest multiple of kCropAlignment that is <= v (proper floor).
 *
 * Used for the clamp bound: clamping against an unaligned bound could return an
 * off-grid origin even when the ideal origin itself was aligned.
 */
int alignFloor(int v) {
    int remainder = v % kCropAlignment;
    if(remainder < 0) {
        remainder += kCropAlignment;
    }
    return v - remainder;
}

// ordered_json preserves insertion order, so the generated convert_params.json
// keeps the same key order as the algorithm team's convert_config.json.
using JsonObject = nlohmann::ordered_json;

void toJson(JsonObject &j, const CameraMode &m) {
    j["name"]        = m.name;
    j["description"] = m.description;
    j["width"]       = m.output_size.width;
    j["height"]      = m.output_size.height;
    j["crop_x"]      = m.transform.crop.x;
    j["crop_y"]      = m.transform.crop.y;
    j["crop_width"]  = m.transform.crop.width;
    j["crop_height"] = m.transform.crop.height;
    j["scale_x"]     = m.transform.scale.x;
    j["scale_y"]     = m.transform.scale.y;
    j["rotate"]      = static_cast<int>(m.transform.rotate);
    j["mirror"]      = m.transform.mirror;
    j["flip"]        = m.transform.flip;
    j["skipping"]    = m.transform.skipping;
    j["binning"]     = m.transform.binning;
    j["lock"]        = m.lock;
}

JsonObject toJson(const CameraConfig &cam) {
    JsonObject j;
    j["id"]            = cam.id;
    j["name"]          = cam.name;
    j["module"]        = cam.module;   // module model, lowercase (multi-product identification)
    j["sensor_width"]  = cam.sensor_size.width;
    j["sensor_height"] = cam.sensor_size.height;
    j["temp_coeff"]    = cam.temp_coeff;   // px/°C, 0 = temperature compensation disabled (§3.7)
    // Read-out mirror/flip (§4.9): required, filled in memory by firmware, and it IS part of
    // the semantic fingerprint (overriding it therefore regenerates convert_params.json) and
    // IS written into convert_params.json — that is exactly how the params copy ends up
    // recording the real read-out state instead of the shipped placeholder.
    JsonObject readout;
    readout["mirror"]       = cam.readout_orientation.mirror;
    readout["flip"]         = cam.readout_orientation.flip;
    j["readout_orientation"] = readout;
    j["modes"]         = JsonObject::array();
    for(const auto &mode : cam.modes) {
        JsonObject modeJson;
        toJson(modeJson, mode);
        j["modes"].push_back(modeJson);
    }
    return j;
}

JsonObject toJson(const ConvertConfig &config) {
    JsonObject j;
    j["format_version"]   = config.format_version;
    j["file_type"]        = config.file_type;
    j["fingerprint"]      = config.fingerprint;
    j["body_fingerprint"] = config.body_fingerprint;
    j["pid"]              = config.pid;   // product PID (multi-product firmware selects by pid + modules)
    j["description"]      = config.description;
    j["cameras"]          = JsonObject::array();
    for(const auto &cam : config.cameras) {
        j["cameras"].push_back(toJson(cam));
    }
    return j;
}

/**
 * @brief Compute a single ImageTransform for a given sensor size and target resolution.
 *
 * Uses center-crop strategy to maintain aspect ratio.
 */
ImageTransform computeTransform(const Size &sensorSize, const Size &targetSize) {
    ImageTransform t;

    double sensorW = static_cast<double>(sensorSize.width);
    double sensorH = static_cast<double>(sensorSize.height);
    double targetW = static_cast<double>(targetSize.width);
    double targetH = static_cast<double>(targetSize.height);

    double sensorAspect = sensorW / sensorH;
    double targetAspect = targetW / targetH;

    double cropW = sensorW;
    double cropH = sensorH;

    // If aspect ratios differ, center-crop the sensor to match target aspect ratio
    if(sensorAspect > targetAspect) {
        // Sensor is wider than target → crop width
        cropW = sensorH * targetAspect;
    } else if(sensorAspect < targetAspect) {
        // Sensor is taller than target → crop height
        cropH = sensorW / targetAspect;
    }

    double cropX = (sensorW - cropW) / 2.0;
    double cropY = (sensorH - cropH) / 2.0;

    // Crop geometry must sit on the Bayer grid (kCropAlignment): the validator rejects an
    // off-grid crop (errors 44/45), so a config generated here must be aligned or it would
    // not even load. This mirrors what the adaptive path in generateParamsJson() does —
    // without it, e.g. 848x480 on a 1600x1300 sensor produced an odd crop height.
    t.crop.width  = std::max(alignFloor(static_cast<int>(std::round(cropW))), kCropAlignment);
    t.crop.height = std::max(alignFloor(static_cast<int>(std::round(cropH))), kCropAlignment);
    t.crop.x      = alignFloor((sensorSize.width - t.crop.width) / 2);
    t.crop.y      = alignFloor((sensorSize.height - t.crop.height) / 2);
    // Defensive: keep the aligned window inside the sensor even for an odd sensor size.
    if(t.crop.x + t.crop.width > sensorSize.width) {
        t.crop.x = alignFloor(sensorSize.width - t.crop.width);
    }
    if(t.crop.y + t.crop.height > sensorSize.height) {
        t.crop.y = alignFloor(sensorSize.height - t.crop.height);
    }

    // Derive the scale from the ALIGNED crop so the size equation (§9.1) holds exactly:
    // width == round(crop_width × scale_x). Computing it from the pre-alignment crop would
    // leave up to ~1 px of error and trip the ±0.5 px consistency check (errors 50/51).
    t.scale.x     = static_cast<double>(targetSize.width) / static_cast<double>(t.crop.width);
    t.scale.y     = static_cast<double>(targetSize.height) / static_cast<double>(t.crop.height);
    t.rotate      = Rotation::DEG_0;
    t.mirror      = false;
    t.flip        = false;
    t.skipping    = 0;
    t.binning     = 0;

    return t;
}

/**
 * @brief Create a mode name from resolution.
 */
std::string makeModeName(int width, int height) {
    std::ostringstream oss;
    oss << width << "x" << height;
    return oss.str();
}

}  // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

ConvertConfig generateConvertJson(const std::vector<CameraConfig> &cameraSpecs, const std::vector<Size> &resolutions) {
    // A camera without modes is rejected by validateConvertJson() (error 22), so fail here
    // instead of handing the caller a config that cannot load.
    if(resolutions.empty()) {
        throw std::invalid_argument("generateConvertJson: resolutions must not be empty (every camera needs >= 1 mode)");
    }
    if(cameraSpecs.empty()) {
        throw std::invalid_argument("generateConvertJson: cameraSpecs must not be empty (error code 11)");
    }

    ConvertConfig config;
    config.format_version   = kFormatVersionCurrent;
    config.file_type        = "config";
    config.fingerprint      = "";
    config.body_fingerprint = "";
    config.pid              = "";   // caller sets the product PID for multi-product firmware
    config.description      = "Multi-resolution mode configuration";

    for(const auto &spec : cameraSpecs) {
        CameraConfig camConfig;
        camConfig.id          = spec.id;
        camConfig.name        = spec.name;
        camConfig.sensor_size = spec.sensor_size;
        camConfig.temp_coeff  = spec.temp_coeff;
        // Product identity and read-out state must survive generation: `module` is the
        // mandatory per-camera identity check (validateConfigIdentity(), error 65 when the
        // config declares none) and readout_orientation is what makes the params copy record
        // the ACTUAL mirror/flip instead of a hard-coded placeholder (design doc §4.9 约定 3/5).
        // Leaving the default here generated a config with module="" and {false,false} even
        // for a mirrored module — the "image looks fine, parameters all wrong" failure mode.
        camConfig.module      = spec.module;
        camConfig.readout_orientation = spec.readout_orientation;

        for(const auto &res : resolutions) {
            CameraMode mode;
            mode.name        = makeModeName(res.width, res.height);
            mode.description = "Auto-generated mode for " + mode.name;
            mode.output_size  = res;
            mode.transform   = computeTransform(spec.sensor_size, res);
            mode.lock       = true;
            camConfig.modes.push_back(mode);
        }

        config.cameras.push_back(camConfig);
    }

    return config;
}

ConvertConfig appendMode(const ConvertConfig &existing, const Size &newResolution) {
    ConvertConfig updated = existing;

    for(auto &cam : updated.cameras) {
        // Check if this resolution already exists
        bool exists = false;
        for(const auto &mode : cam.modes) {
            if(mode.output_size.width == newResolution.width && mode.output_size.height == newResolution.height) {
                exists = true;
                break;
            }
        }

        if(!exists) {
            CameraMode mode;
            mode.name        = makeModeName(newResolution.width, newResolution.height);
            mode.description = "Auto-generated mode for " + mode.name;
            mode.output_size  = newResolution;
            mode.transform   = computeTransform(cam.sensor_size, newResolution);
            mode.lock       = true;
            cam.modes.push_back(mode);
        }
    }

    return updated;
}

namespace {

/**
 * @brief FNV-1a 64-bit hash (deterministic across platforms).
 */
uint64_t fnv1a64(const std::string &data) {
    uint64_t hash = 14695981039346656037ULL;
    for(unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

/**
 * @brief Quantize temp_coeff to an integer for deterministic fingerprinting.
 *
 * temp_coeff (px/°C) is a floating point, but the fingerprint must be bit-identical
 * across platforms: printf %g/%f formatting is not guaranteed identical across libc
 * implementations (exponent digit count, locale decimal separator), which is exactly
 * why scale_x/scale_y were excluded from the hash. Quantizing to 1e-6 px/°C keeps the
 * fingerprint:
 *  - fully deterministic (pure integer, no float formatting / locale involved);
 *  - insensitive to sub-1e-6 float noise (authoring "0.35" vs "0.3500000001" hashes
 *    identically, so insignificant edits do not spuriously trigger regeneration);
 *  - sensitive to any meaningful coefficient change (1e-6 px/°C over ΔT=10°C is
 *    1e-5 px — physically negligible).
 */
int64_t quantizeTempCoeff(double v) {
    return static_cast<int64_t>(std::llround(v * 1e6));
}

/**
 * @brief Build the canonical semantic string a config is fingerprinted by.
 *
 * Covers the fields that flow into params (design doc §3.1): format_version,
 * per-camera id/name/sensor_width/sensor_height/temp_coeff/readout_orientation,
 * per-mode name/width/height/crop_x/crop_y/crop_width/crop_height/rotate/mirror/flip/
 * skipping/binning/lock. scale_x/scale_y (floating point), description and fingerprint
 * are excluded.
 *
 * temp_coeff is included (quantized to an integer, see quantizeTempCoeff) because it
 * directly changes the output focal length and is read from the params copy at runtime,
 * so the firmware must detect config changes to it (§3.7).
 *
 * readout_orientation is included on purpose (§4.9): firmware fills it in memory from
 * the sensor read-out registers, so a change here means the device's read-out state
 * changed and the stored convert_params.json must be regenerated — that is exactly what
 * makes the params copy record the ACTUAL state instead of the placeholder shipped in
 * the config.
 */
std::string canonicalSemanticString(const ConvertConfig &config) {
    std::ostringstream oss;
    oss << "format_version=" << config.format_version << ";";
    // Product identity: a single firmware may embed several configs that differ only by pid
    // and/or by the per-camera module model, so both take part in the semantic hash (an empty
    // value simply hashes as empty).
    oss << "pid=" << config.pid << ";";
    for(size_t ci = 0; ci < config.cameras.size(); ++ci) {
        const CameraConfig &cam = config.cameras[ci];
        if(ci > 0) {
            oss << ";";
        }
        oss << "camera[id=" << cam.id
            << ",name=" << cam.name
            << ",module=" << cam.module
            << ",sensor=" << cam.sensor_size.width << "x" << cam.sensor_size.height
            << ",temp_coeff=" << quantizeTempCoeff(cam.temp_coeff)
            // Read-out state, filled by firmware in memory (§4.9). Fingerprinted on
            // purpose so a change regenerates convert_params.json and the params copy
            // ends up recording the actual state.
            << ",readout_mirror=" << (cam.readout_orientation.mirror ? 1 : 0)
            << ",readout_flip=" << (cam.readout_orientation.flip ? 1 : 0)
            << ",modes=";
        for(size_t mi = 0; mi < cam.modes.size(); ++mi) {
            const CameraMode &m = cam.modes[mi];
            if(mi > 0) {
                oss << "|";
            }
            oss << "mode[name=" << m.name
                << ",width=" << m.output_size.width
                << ",height=" << m.output_size.height
                << ",crop_x=" << m.transform.crop.x
                << ",crop_y=" << m.transform.crop.y
                << ",crop_width=" << m.transform.crop.width
                << ",crop_height=" << m.transform.crop.height
                << ",rotate=" << static_cast<int>(m.transform.rotate)
                << ",mirror=" << (m.transform.mirror ? 1 : 0)
                << ",flip=" << (m.transform.flip ? 1 : 0)
                << ",skipping=" << m.transform.skipping
                << ",binning=" << m.transform.binning
                << ",lock=" << (m.lock ? 1 : 0)
                << "]";
        }
        oss << "]";
    }
    return oss.str();
}

}  // anonymous namespace

std::string computeFingerprint(const ConvertConfig &config) {
    uint64_t h = fnv1a64(canonicalSemanticString(config));
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
    return std::string(buf);
}

ConvertConfig generateParamsJson(const ConvertConfig &config, const CalibrationData &calib,
                                 const SensorSizeConfig &actualSensorSizes) {
    ConvertConfig result = config;
    result.file_type   = "params";
    result.fingerprint = computeFingerprint(config);

    for(auto &cam : result.cameras) {
        // The actual sensor resolution only feeds the adaptive computation; the declared
        // sensor fields are copied from the config and stay untouched. A device may mix
        // sensor resolutions, so this is looked up (and checked) per camera id.
        const CameraSensorSize *actualSensor = nullptr;
        for(const auto &entry : actualSensorSizes.cameras) {
            if(entry.camera_id == cam.id) {
                actualSensor = &entry;
                break;
            }
        }
        if(actualSensor == nullptr) {
            throw std::invalid_argument("camera " + cam.id
                                    + ": no actual sensor resolution given (every camera must be reported)");
        }
        if(cam.sensor_size.width != actualSensor->sensor_size.width
           || cam.sensor_size.height != actualSensor->sensor_size.height) {
            throw std::invalid_argument("camera " + cam.id
                                    + ": actual sensor " + std::to_string(actualSensor->sensor_size.width) + "x"
                                    + std::to_string(actualSensor->sensor_size.height) + " differs from declared "
                                    + std::to_string(cam.sensor_size.width) + "x"
                                    + std::to_string(cam.sensor_size.height));
        }

        // From here on, this camera's own sensor size drives anchoring and clamping.
        const Size sensorSize = cam.sensor_size;

        const CameraParams *calibCam = nullptr;
        for(const auto &cp : calib.cameras) {
            if(cp.camera_id == cam.id) {
                calibCam = &cp;
                break;
            }
        }
        if(calibCam == nullptr) {
            throw std::invalid_argument("camera " + cam.id + ": calibration data missing");
        }

        // The calibration (params_cam.yaml) may have been done on a centred sub-window
        // of the sensor (calib image <= sensor, even difference, no scaling) — e.g. the
        // early TN2608-A-250201157 units calibrated at 1600x1200 on a 1920x1200 sensor.
        // Crops below live in the SENSOR frame, so anchor the calibration first; a
        // calibration larger than the sensor, or one whose difference is odd (so no
        // centred offset exists), is a hard error (design doc §4.10).
        CameraParams anchored;
        try {
            anchored = anchorCalibrationToSensor(*calibCam, sensorSize);
        } catch(const std::invalid_argument &e) {
            throw std::invalid_argument("camera " + cam.id + ": " + e.what());
        }

        double cx = anchored.intrinsics.cx;
        double cy = anchored.intrinsics.cy;

        // Step 1: base modes = no magnification (unity scale && no skipping/binning)
        std::vector<size_t> baseIdx;
        for(size_t i = 0; i < cam.modes.size(); ++i) {
            const CameraMode &m = cam.modes[i];
            if(isUnityScale(m.transform.scale.x) && isUnityScale(m.transform.scale.y)
               && m.transform.skipping == 0 && m.transform.binning == 0) {
                baseIdx.push_back(i);
            }
        }
        if(baseIdx.empty()) {
            throw std::invalid_argument("camera " + cam.id + ": no base mode (config error)");
        }

        // Step 2: base modes with lock=false recompute the crop origin by centering
        // the calibration principal point (rotate 90°/270° swap output axes).
        for(size_t i : baseIdx) {
            CameraMode &b = cam.modes[i];
            if(b.lock) {
                continue;
            }
            double idealCropX, idealCropY;
            int rot = static_cast<int>(b.transform.rotate);
            if(rot == 0 || rot == 180) {
                idealCropX = cx - b.output_size.width / 2.0;
                idealCropY = cy - b.output_size.height / 2.0;
            } else {
                idealCropX = cx - b.output_size.height / 2.0;
                idealCropY = cy - b.output_size.width / 2.0;
            }
            // Crop origins must land on the Bayer grid (design doc §4.5). The
            // clamp bound is aligned down as well: clamping against an unaligned
            // bound could return an off-grid origin even for an aligned ideal.
            int maxX = alignFloor(sensorSize.width - b.transform.crop.width);
            int maxY = alignFloor(sensorSize.height - b.transform.crop.height);
            b.transform.crop.x = std::min(std::max(alignRound(idealCropX), 0), maxX);
            b.transform.crop.y = std::min(std::max(alignRound(idealCropY), 0), maxY);
        }

        // Step 3: derived modes with lock=false inherit the crop origin of the base
        // mode that matches (crop size + orientation) in the config.
        for(size_t i = 0; i < cam.modes.size(); ++i) {
            CameraMode &m = cam.modes[i];
            bool isBase = false;
            for(size_t bi : baseIdx) {
                if(bi == i) {
                    isBase = true;
                    break;
                }
            }
            if(isBase || m.lock) {
                continue;
            }
            const CameraMode *base = nullptr;
            for(size_t bi : baseIdx) {
                const CameraMode &b = cam.modes[bi];
                if(b.transform.crop.width == m.transform.crop.width
                   && b.transform.crop.height == m.transform.crop.height
                   && b.transform.rotate == m.transform.rotate
                   && b.transform.mirror == m.transform.mirror
                   && b.transform.flip == m.transform.flip) {
                    base = &b;
                    break;
                }
            }
            if(base == nullptr) {
                throw std::invalid_argument("camera " + cam.id + ": derived mode '" + m.name
                                        + "' has no matching (crop size + orientation) base mode (config error)");
            }
            m.transform.crop.x = base->transform.crop.x;
            m.transform.crop.y = base->transform.crop.y;
        }
    }

    // The body hash covers the values THIS file actually stores — i.e. after the adaptive
    // crops above were applied — so firmware can tell a tampered/corrupted body apart from
    // an intact one (§3.6 check b2). It cannot be derived from `fingerprint`, which binds
    // the params to their SOURCE config and therefore differs from the adapted body
    // whenever a lock=false mode was recomputed.
    result.body_fingerprint = computeFingerprint(result);

    return result;
}

ConvertConfig generateParamsJson(const ConvertConfig &config, const CalibrationData &calib, const Size &sensorSize) {
    // Homogeneous case: the same sensor resolution for every camera of the device.
    SensorSizeConfig actualSensorSizes;
    for(const auto &cam : config.cameras) {
        actualSensorSizes.set(cam.id, sensorSize.width, sensorSize.height);
    }
    return generateParamsJson(config, calib, actualSensorSizes);
}

std::string toJsonString(const ConvertConfig &config, bool pretty) {
    JsonObject j = toJson(config);
    return pretty ? j.dump(2) : j.dump();
}

bool saveConvertConfigFile(const std::string &path, const ConvertConfig &config, bool pretty) {
    return writeTextFile(path, toJsonString(config, pretty));
}

}  // namespace camadapt

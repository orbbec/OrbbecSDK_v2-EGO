/**
 * @file YamlParser.hpp
 * @brief Internal minimal YAML parsers for params_cam.yaml / params_imu.yaml.
 *
 * These are line-by-line state machine parsers that handle the specific calibration
 * file formats. They are NOT general-purpose YAML parsers.
 */

#pragma once

#include "camadapt/Types.h"

#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace camadapt {
namespace internal {

// ============================================================================
// Shared helpers
// ============================================================================

inline bool yamlIsComment(const std::string &line) {
    size_t pos = line.find_first_not_of(" \t");
    return (pos != std::string::npos && line[pos] == '#');
}

inline std::string yamlStrip(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    if(start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}

inline std::string yamlStripQuotes(const std::string &s) {
    if(s.size() >= 2 && s.front() == '"' && s.back() == '"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

inline bool yamlIsSequenceItem(const std::string &stripped) {
    return stripped.size() >= 2 && stripped[0] == '-' && stripped[1] == ' ';
}

/**
 * @brief Parse an inline sequence like [1.0, 0.0, 0.0] into doubles.
 */
inline std::vector<double> yamlParseInlineSequence(const std::string &s) {
    std::vector<double> result;
    std::string       inner = s;
    if(!inner.empty() && inner.front() == '[') inner.erase(0, 1);
    if(!inner.empty() && inner.back() == ']') inner.pop_back();

    std::istringstream ss(inner);
    std::string        token;
    while(std::getline(ss, token, ',')) {
        token = yamlStrip(token);
        if(!token.empty()) {
            result.push_back(std::stod(token));
        }
    }
    return result;
}

// ============================================================================
// params_cam.yaml Parser State Machine
// ============================================================================

enum class CamYamlState {
    TOP,
    IN_CALIBRATION_INFO,   // skipped (calibration record, not modeled, §5.1)
    IN_ENVIRONMENT,        // skipped (calibration record, not modeled, §5.1)
    IN_ENVIRONMENT_TEMPERATURE,  // environment.temperature: per-camera calibration temperature (§3.7)
    IN_CAMERAS,
    IN_CAMERA,
    IN_CAMERA_INTRINSICS,
    IN_CAMERA_DISTORTION,
    IN_CAMERA_EXTRINSICS,
    IN_EXTRINSICS_ROTATION,
};

/**
 * @brief Minimal line-by-line parser for params_cam.yaml.
 *
 * Only the camera section is captured; calibration_info is skipped and the
 * environment block is skipped except environment.temperature, whose per-camera
 * calibration temperature (sensor id → value_t) is stored on CameraParams
 * (design doc §5.1 / §3.7). The parsed CalibrationData starts from cameras.
 */
class YamlParser {
public:
    YamlParser() : state_(CamYamlState::TOP), currentCamera_(nullptr), rotationRow_(0) {}

    CalibrationData parse(const std::string &content) {
        std::istringstream stream(content);
        std::string        line;
        int                lineNum = 0;

        while(std::getline(stream, line)) {
            lineNum++;

            // Strip trailing \r
            if(!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip empty lines and comments
            if(line.empty() || yamlIsComment(line)) {
                continue;
            }

            try {
                parseLine(line);
            } catch(const std::exception &e) {
                throw std::runtime_error("Line " + std::to_string(lineNum) + ": " + e.what());
            }
        }

        // The environment.temperature block precedes the cameras section, so the
        // per-camera calibration temperature is applied after all cameras are parsed.
        for(auto &cam : data_.cameras) {
            auto it = calibTemps_.find(cam.camera_id);
            cam.calibration_temperature =
                (it != calibTemps_.end()) ? it->second : std::numeric_limits<double>::quiet_NaN();
        }

        return data_;
    }

private:
    CalibrationData           data_;
    CamYamlState              state_;
    CameraParams *            currentCamera_;
    int                       rotationRow_;
    std::map<std::string, double> calibTemps_;      ///< environment.temperature: sensor id → °C (§3.7)
    std::string               currentTempSensor_;   ///< sensor id of the temperature item being parsed

    void parseLine(const std::string &line) {
        std::string stripped = yamlStrip(line);

        if(yamlIsSequenceItem(stripped)) {
            parseSequenceItem(stripped);
            return;
        }

        size_t colonPos = stripped.find(':');
        if(colonPos == std::string::npos) {
            return;  // Skip lines without colon
        }

        std::string key = yamlStrip(stripped.substr(0, colonPos));
        std::string value;
        if(colonPos + 1 < stripped.size()) {
            value = yamlStrip(stripped.substr(colonPos + 1));
        }

        handleKeyValue(key, value);
    }

    void handleKeyValue(const std::string &key, const std::string &value) {
        // Top-level section keys
        if(key == "calibration_info") {
            state_ = CamYamlState::IN_CALIBRATION_INFO;
            return;
        }
        if(key == "environment") {
            state_ = CamYamlState::IN_ENVIRONMENT;
            return;
        }
        if(key == "cameras") {
            state_ = CamYamlState::IN_CAMERAS;
            return;
        }

        // Sub-block transitions within a camera section
        if(key == "intrinsics") {
            state_ = CamYamlState::IN_CAMERA_INTRINSICS;
            return;
        }
        if(key == "distortion") {
            state_ = CamYamlState::IN_CAMERA_DISTORTION;
            return;
        }
        if(key == "extrinsics") {
            state_ = CamYamlState::IN_CAMERA_EXTRINSICS;
            return;
        }
        if(key == "temperature") {
            // environment.temperature: per-camera calibration temperature (§3.7)
            state_ = CamYamlState::IN_ENVIRONMENT_TEMPERATURE;
            return;
        }

        switch(state_) {
        case CamYamlState::IN_ENVIRONMENT_TEMPERATURE:
            // "value_t: 32.48" lines inside "- sensor: ..." items (§3.7)
            if(key == "value_t" && !currentTempSensor_.empty()) {
                calibTemps_[currentTempSensor_] = std::stod(value);
            }
            break;
        case CamYamlState::IN_CAMERA:
            handleCamera(key, value);
            break;
        case CamYamlState::IN_CAMERA_INTRINSICS:
            handleIntrinsics(key, value);
            break;
        case CamYamlState::IN_CAMERA_DISTORTION:
            handleDistortion(key, value);
            break;
        case CamYamlState::IN_CAMERA_EXTRINSICS:
            handleExtrinsics(key, value);
            break;
        case CamYamlState::IN_EXTRINSICS_ROTATION:
            // The three rotation rows are sequence items, so nothing else moves the state
            // out of IN_EXTRINSICS_ROTATION: the following "translation: [...]" key:value
            // line must still be routed to the extrinsics handler (regression: cam_1's
            // non-zero translation was silently dropped).
            //
            // Any OTHER key ends the extrinsics block and belongs to the camera level
            // again — design doc §5.1 places camera-level keys such as `vcm_code` after
            // `extrinsics` (that is also the order toYaml() writes). Swallowing them here
            // lost them (regression: vcm_code always read back as 0 and the
            // toYaml() → parseParamsYaml() round trip dropped it), so re-dispatch the key
            // through the normal camera-field path.
            if(key == "rotation" || key == "translation") {
                handleExtrinsics(key, value);
                break;
            }
            state_ = CamYamlState::IN_CAMERA;
            handleKeyValue(key, value);
            break;
        default:
            break;  // calibration_info / environment / other: skipped
        }
    }

    void parseSequenceItem(const std::string &stripped) {
        std::string content = yamlStrip(stripped.substr(1));  // Remove leading "- "

        switch(state_) {
        case CamYamlState::IN_ENVIRONMENT_TEMPERATURE: {
            // "- sensor: \"cam_0\"" / "- value_t: 32.48" pairs (§3.7)
            size_t colonPos = content.find(':');
            if(colonPos != std::string::npos) {
                std::string k = yamlStrip(content.substr(0, colonPos));
                std::string v = yamlStrip(content.substr(colonPos + 1));
                if(k == "sensor") {
                    currentTempSensor_ = yamlStripQuotes(v);
                } else if(k == "value_t" && !currentTempSensor_.empty()) {
                    calibTemps_[currentTempSensor_] = std::stod(v);
                }
            }
            return;
        }
        case CamYamlState::IN_CAMERAS:
        case CamYamlState::IN_CAMERA:
        case CamYamlState::IN_CAMERA_INTRINSICS:
        case CamYamlState::IN_CAMERA_DISTORTION:
        case CamYamlState::IN_CAMERA_EXTRINSICS:
        case CamYamlState::IN_EXTRINSICS_ROTATION: {
            // Rotation matrix rows in IN_EXTRINSICS_ROTATION start with "["
            if(state_ == CamYamlState::IN_EXTRINSICS_ROTATION
               && !content.empty() && content.front() == '[') {
                std::vector<double> row = yamlParseInlineSequence(content);
                if(row.size() == 3 && currentCamera_ && rotationRow_ < 3) {
                    for(int c = 0; c < 3; ++c) {
                        currentCamera_->extrinsics.rotation.m[rotationRow_][c] = row[c];
                    }
                    rotationRow_++;
                }
                return;
            }

            // Otherwise, start a new camera item
            state_ = CamYamlState::IN_CAMERA;
            CameraParams cam;
            data_.cameras.push_back(cam);
            currentCamera_ = &data_.cameras.back();

            size_t colonPos = content.find(':');
            if(colonPos != std::string::npos) {
                std::string k = yamlStrip(content.substr(0, colonPos));
                std::string v = yamlStrip(content.substr(colonPos + 1));
                handleCamera(k, v);
            }
            return;
        }
        default:
            return;
        }
    }

    void handleCamera(const std::string &key, const std::string &value) {
        if(!currentCamera_) return;
        if(key == "id") currentCamera_->camera_id = yamlStripQuotes(value);
        else if(key == "name") currentCamera_->camera_name = yamlStripQuotes(value);
        else if(key == "distortion_model") currentCamera_->distortion.model = yamlStripQuotes(value);
        else if(key == "image_width") currentCamera_->image_size.width = std::stoi(value);
        else if(key == "image_height") currentCamera_->image_size.height = std::stoi(value);
        else if(key == "vcm_code") currentCamera_->vcm_code = std::stoi(value);
        else if(key == "intrinsics") state_ = CamYamlState::IN_CAMERA_INTRINSICS;
        else if(key == "distortion") state_ = CamYamlState::IN_CAMERA_DISTORTION;
        else if(key == "extrinsics") state_ = CamYamlState::IN_CAMERA_EXTRINSICS;
    }

    void handleIntrinsics(const std::string &key, const std::string &value) {
        if(!currentCamera_) return;
        if(key == "fx") currentCamera_->intrinsics.fx = std::stod(value);
        else if(key == "fy") currentCamera_->intrinsics.fy = std::stod(value);
        else if(key == "cx") currentCamera_->intrinsics.cx = std::stod(value);
        else if(key == "cy") currentCamera_->intrinsics.cy = std::stod(value);
    }

    void handleDistortion(const std::string &key, const std::string &value) {
        if(!currentCamera_) return;
        if(key == "k1") currentCamera_->distortion.k1 = std::stod(value);
        else if(key == "k2") currentCamera_->distortion.k2 = std::stod(value);
        else if(key == "k3") currentCamera_->distortion.k3 = std::stod(value);
        else if(key == "k4") currentCamera_->distortion.k4 = std::stod(value);
        else if(key == "k5") currentCamera_->distortion.k5 = std::stod(value);
        else if(key == "k6") currentCamera_->distortion.k6 = std::stod(value);
        else if(key == "p1") currentCamera_->distortion.p1 = std::stod(value);
        else if(key == "p2") currentCamera_->distortion.p2 = std::stod(value);
    }

    void handleExtrinsics(const std::string &key, const std::string &value) {
        if(!currentCamera_) return;
        if(key == "rotation") {
            state_       = CamYamlState::IN_EXTRINSICS_ROTATION;
            rotationRow_ = 0;
        } else if(key == "translation") {
            std::vector<double> t = yamlParseInlineSequence(value);
            if(t.size() == 3) {
                currentCamera_->extrinsics.translation.x = t[0];
                currentCamera_->extrinsics.translation.y = t[1];
                currentCamera_->extrinsics.translation.z = t[2];
            }
        }
    }
};

// ============================================================================
// params_imu.yaml Parser State Machine
// ============================================================================

enum class ImuYamlState {
    TOP,
    IN_CAM0,
    IN_CAM0_T_CAM_IMU,
    IN_IMU0,   // passthrough: captured verbatim
};

/**
 * @brief Minimal line-by-line parser for params_imu.yaml.
 *
 * Models the cam0 block (intrinsics, resolution, distortion, T_cam_imu,
 * timeshift) and the gravity vector; the imu0 block is captured verbatim as
 * passthrough text (design doc §5.2).
 */
class ImuYamlParser {
public:
    CameraImuParams parse(const std::string &content) {
        std::istringstream stream(content);
        std::string        line;
        int                lineNum = 0;

        while(std::getline(stream, line)) {
            lineNum++;

            if(!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Continuation of a multi-line inline sequence ([...] wrapped across
            // lines, as in the OpenCV-style result-imu.yaml). Accumulate until
            // the ']' closes the bracket.
            if(!pendingValue_.empty()) {
                if(yamlIsComment(line)) {
                    continue;
                }
                pendingValue_ += " ";
                pendingValue_ += yamlStrip(line);
                if(pendingValue_.back() == ']') {
                    finalizePendingSequence();
                }
                continue;
            }

            // imu0 passthrough: capture every indented line verbatim until a
            // top-level key (no leading whitespace) ends the block.
            if(state_ == ImuYamlState::IN_IMU0) {
                if(!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
                    data_.imu0_text += line;
                    data_.imu0_text += '\n';
                    continue;
                }
                state_ = ImuYamlState::TOP;
            }

            if(line.empty() || yamlIsComment(line)) {
                continue;
            }

            try {
                parseLine(line);
            } catch(const std::exception &e) {
                throw std::runtime_error("Line " + std::to_string(lineNum) + ": " + e.what());
            }
        }

        // Drop the trailing newline of the captured imu0 block
        while(!data_.imu0_text.empty() && data_.imu0_text.back() == '\n') {
            data_.imu0_text.pop_back();
        }
        return data_;
    }

private:
    CameraImuParams data_;
    ImuYamlState   state_;
    int            tRow_;
    std::string    pendingValue_;  // multi-line inline sequence being accumulated
    std::string    pendingKey_;    // owning key ("" = T_cam_imu row)

    // ------------------------------------------------------------------
    // Multi-line inline sequence support ([...] wrapped across lines)
    // ------------------------------------------------------------------

    static bool startsBracketUnclosed(const std::string &value) {
        return !value.empty() && value.front() == '[' && value.back() != ']';
    }

    void finalizePendingSequence() {
        std::string value = pendingValue_;
        std::string key   = pendingKey_;
        pendingValue_.clear();
        pendingKey_.clear();

        if(key.empty()) {
            // A T_cam_imu matrix row
            std::vector<double> row = yamlParseInlineSequence(value);
            if(row.size() == 4 && tRow_ < 3) {
                for(int c = 0; c < 3; ++c) {
                    data_.T_cam_imu.rotation.m[tRow_][c] = row[c];
                }
                if(tRow_ == 0) data_.T_cam_imu.translation.x = row[3];
                else if(tRow_ == 1) data_.T_cam_imu.translation.y = row[3];
                else if(tRow_ == 2) data_.T_cam_imu.translation.z = row[3];
                tRow_++;
            }
            return;
        }

        if(key == "gravity") {
            std::vector<double> g = yamlParseInlineSequence(value);
            if(g.size() == 3) {
                data_.gravity = g;
            }
            return;
        }
        finalizeCam0Sequence(key, value);
    }

    void parseLine(const std::string &line) {
        std::string stripped = yamlStrip(line);

        if(yamlIsSequenceItem(stripped)) {
            std::string content = yamlStrip(stripped.substr(1));
            if(state_ == ImuYamlState::IN_CAM0_T_CAM_IMU) {
                if(startsBracketUnclosed(content)) {
                    pendingValue_ = content;
                    pendingKey_.clear();  // a T_cam_imu row
                } else {
                    std::vector<double> row = yamlParseInlineSequence(content);
                    if(row.size() == 4 && tRow_ < 3) {
                        for(int c = 0; c < 3; ++c) {
                            data_.T_cam_imu.rotation.m[tRow_][c] = row[c];
                        }
                        if(tRow_ == 0) data_.T_cam_imu.translation.x = row[3];
                        else if(tRow_ == 1) data_.T_cam_imu.translation.y = row[3];
                        else if(tRow_ == 2) data_.T_cam_imu.translation.z = row[3];
                        tRow_++;
                    }
                }
            }
            return;
        }

        size_t colonPos = stripped.find(':');
        if(colonPos == std::string::npos) {
            return;
        }

        std::string key = yamlStrip(stripped.substr(0, colonPos));
        std::string value;
        if(colonPos + 1 < stripped.size()) {
            value = yamlStrip(stripped.substr(colonPos + 1));
        }

        if(key == "cam0") {
            state_ = ImuYamlState::IN_CAM0;
            return;
        }
        if(key == "imu0") {
            state_ = ImuYamlState::IN_IMU0;
            return;
        }
        if(key == "gravity") {
            if(startsBracketUnclosed(value)) {
                pendingValue_ = value;
                pendingKey_   = "gravity";
            } else {
                std::vector<double> g = yamlParseInlineSequence(value);
                if(g.size() == 3) {
                    data_.gravity = g;
                }
            }
            return;
        }

        // cam0 scalar/list keys may appear while still inside the T_cam_imu block
        // (the sequence ends as soon as a non "- [" line shows up).
        if(state_ == ImuYamlState::IN_CAM0 || state_ == ImuYamlState::IN_CAM0_T_CAM_IMU) {
            handleCam0(key, value);
        }
    }

    void handleCam0(const std::string &key, const std::string &value) {
        if(key == "T_cam_imu") {
            state_ = ImuYamlState::IN_CAM0_T_CAM_IMU;
            tRow_  = 0;
            data_.cam0_order.push_back(key);
        } else if(key == "timeshift_cam_imu") {
            data_.timeshift_cam_imu = std::stod(value);
            data_.cam0_order.push_back(key);
        } else if(key == "intrinsics" || key == "distortion_coeffs" || key == "resolution") {
            data_.cam0_order.push_back(key);
            if(startsBracketUnclosed(value)) {
                pendingValue_ = value;
                pendingKey_   = key;
            } else {
                finalizeCam0Sequence(key, value);
            }
        } else if(key == "distortion_model") {
            data_.distortion_model = yamlStripQuotes(value);
            data_.cam0_order.push_back(key);
        } else {
            // Non-modeled cam0 field (camera_model / lineDelay / rostopic /
            // baseline ...): preserve it for re-output (design doc §5.2).
            if(startsBracketUnclosed(value)) {
                pendingValue_ = value;
                pendingKey_   = key;
            } else {
                data_.cam0_order.push_back(key + ": " + value);
            }
        }
    }

    void finalizeCam0Sequence(const std::string &key, const std::string &value) {
        if(key == "intrinsics") {
            std::vector<double> k = yamlParseInlineSequence(value);
            if(k.size() == 4) {
                // params_imu.yaml order is [cx, cy, fx, fy] (OpenCV-style IMU
                // format); the struct stores fx/fy/cx/cy separately.
                data_.intrinsics.cx = k[0];
                data_.intrinsics.cy = k[1];
                data_.intrinsics.fx = k[2];
                data_.intrinsics.fy = k[3];
            }
        } else if(key == "distortion_coeffs") {
            data_.distortion_coeffs = yamlParseInlineSequence(value);
        } else if(key == "resolution") {
            std::vector<double> r = yamlParseInlineSequence(value);
            if(r.size() == 2) {
                data_.resolution.width  = static_cast<int>(r[0]);
                data_.resolution.height = static_cast<int>(r[1]);
            }
        } else {
            // Non-modeled cam0 field with a (possibly multi-line) value
            data_.cam0_order.push_back(key + ": " + value);
        }
    }
};

// ============================================================================
// params_imu.yaml IMU-intrinsics Parser (imu0 block)
// ============================================================================

/**
 * @brief Minimal line-by-line parser for the IMU's own intrinsics in the `imu0`
 *        block of params_imu.yaml (design doc §5.2).
 *
 * Completely self-contained and independent from ImuYamlParser: it walks the imu0
 * subtree (nested `accelerometer` / `gyroscope` blocks, `- [ ... ]` matrix rows and
 * inline sequences wrapped across lines) plus the top-level `gravity` vector, and
 * ignores everything else. Indentation is used only to tell "still inside imu0"
 * from "back at the top level" (a key at or above the `imu0:` indent ends the
 * block), so both indentation styles found in the shipped configs parse: 3 spaces
 * (configs/EGO-2/DCD111-*) and 2 spaces (configs/EGO-4B/DCD111-523).
 *
 * Tolerant of missing keys - a key that is not there keeps its default - and of
 * malformed rows/sequences under unmodeled matrices (T_imu_body, Ma_gyr), which are
 * skipped. Only a malformed numeric value under a recognized key throws, exactly
 * like ImuYamlParser.
 *
 * A modeled 3x3 (M_acc / M_gyr / C_gyro_i / temp_slope) is REJECTED AS A WHOLE when
 * its content is malformed, instead of silently keeping a partially filled matrix:
 * a `- [ ... ]` row with a number count other than 3, an inline sequence with a
 * number count other than 9 and an inline sequence left unterminated at end of file
 * all throw std::runtime_error (with the "Line N: " prefix). Both matrix spellings
 * are accepted: three `- [ a, b, c ]` rows, or the inline 9-number form
 * `key: [ a, b, c, d, e, f, g, h, i ]` (which may also wrap across lines).
 *
 * A noise sub-block (`accelerometer` / `gyroscope`) is only closed by a DEDENT (a key
 * at or left of the sub-block's own indent) or by a new known sub-block - never by an
 * unrecognised key inside it, so keys a future file adds do not silently shadow the
 * recognised keys that follow them.
 *
 * `gravity` is only recognised OUTSIDE the imu0 subtree: an `imu0.gravity` (IMU frame)
 * is never taken for the unified world-frame vector.
 */
class ImuIntrinsicsParser {
public:
    ImuIntrinsics parse(const std::string &content) {
        initDefaults();

        std::istringstream stream(content);
        std::string        line;
        int                lineNum = 0;

        while(std::getline(stream, line)) {
            lineNum++;

            if(!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // The continuation path is INSIDE the try: a malformed token in a wrapped
            // sequence must report the same "Line N: ..." std::runtime_error as a
            // single-line value, instead of escaping as a raw std::invalid_argument.
            try {
                // Continuation of an inline sequence / matrix row wrapped across lines
                // (params_imu.yaml wraps gravity, AccBias, GyrBias and most matrix rows).
                if(!pendingValue_.empty()) {
                    if(yamlIsComment(line)) {
                        continue;
                    }
                    pendingValue_ += " ";
                    pendingValue_ += yamlStrip(line);
                    if(!pendingValue_.empty() && pendingValue_.back() == ']') {
                        finalizePending();
                    }
                    continue;
                }

                if(line.empty() || yamlIsComment(line)) {
                    continue;
                }

                parseLine(line);
            } catch(const std::exception &e) {
                throw std::runtime_error("Line " + std::to_string(lineNum) + ": " + e.what());
            }
        }

        // An inline sequence still open at end of file is structurally malformed:
        // report it rather than silently keeping the default of a half-filled value.
        if(!pendingValue_.empty()) {
            throw std::runtime_error("Line " + std::to_string(lineNum) + ": unterminated inline sequence for '"
                                     + pendingKey_ + "'");
        }

        // M_acc is flattened row-major and used as-is: AccCalib = M_acc * AccRaw - AccBias.
        for(int i = 0; i < 9; ++i) {
            result_.accel.scale_misalignment[i] = mAcc_[i];
        }
        // GyrCalib = M_gyr * (C_gyro_i * GyrRaw) - GyroBias, so the flattened
        // row-major scale/misalignment matrix is the product M_gyr * C_gyro_i.
        // Both factors default to the identity, so an imu0 block without them yields
        // the identity - not a zero matrix, which would nullify the signal.
        multiply3(mGyr_, cGyroI_, result_.gyro.scale_misalignment);
        return result_;
    }

private:
    ImuIntrinsics result_;
    bool          inImu0_;       ///< inside the imu0 subtree
    size_t        imu0Indent_;   ///< indent of the `imu0:` key (block ends at or above it)
    std::string   blockName_;    ///< "accelerometer" / "gyroscope" ("" = none)
    size_t        blockIndent_;  ///< indent of the sub-block key (an unknown key deeper than this stays in the block)
    std::string   matrixKey_;    ///< M_acc / M_gyr / C_gyro_i / temp_slope whose rows follow ("" = none)
    double       *matrixTarget_; ///< destination of matrixKey_, or nullptr for an unmodeled matrix
    int           matrixRow_;    ///< next row index of matrixKey_
    double        mAcc_[9];      ///< imu0.M_acc (identity by default)
    double        mGyr_[9];      ///< imu0.M_gyr (identity by default)
    double        cGyroI_[9];    ///< imu0.C_gyro_i (identity by default)
    std::string   pendingValue_;  ///< inline sequence being accumulated across lines
    std::string   pendingKey_;    ///< owning key of pendingValue_
    int           pendingRow_;    ///< row index when pendingValue_ is a matrix row, else -1

    // ------------------------------------------------------------------
    // Small helpers
    // ------------------------------------------------------------------

    static size_t indentOf(const std::string &line) {
        size_t i = 0;
        while(i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        return i;
    }

    static bool startsBracketUnclosed(const std::string &value) {
        return !value.empty() && value.front() == '[' && value.back() != ']';
    }

    static bool isNoiseKey(const std::string &key) {
        return key == "noise_density" || key == "random_walk" || key == "reference_temp";
    }

    static void identity9(double (&m)[9]) {
        for(int i = 0; i < 9; ++i) {
            m[i] = 0.0;
        }
        m[0] = 1.0;
        m[4] = 1.0;
        m[8] = 1.0;
    }

    static void multiply3(const double (&a)[9], const double (&b)[9], double (&out)[9]) {
        for(int r = 0; r < 3; ++r) {
            for(int c = 0; c < 3; ++c) {
                double sum = 0.0;
                for(int k = 0; k < 3; ++k) {
                    sum += a[r * 3 + k] * b[k * 3 + c];
                }
                out[r * 3 + c] = sum;
            }
        }
    }

    /// Every field explicitly initialized: scale_misalignment to the identity (see
    /// the header of parseImuIntrinsics), temp_slope / bias / gravity / noise to 0,
    /// mag to all zeros with has_mag = false.
    void initDefaults() {
        AccelIntrinsics &accel = result_.accel;
        accel.noise_density  = 0.0;
        accel.random_walk    = 0.0;
        accel.reference_temp = 0.0;
        for(int i = 0; i < 3; ++i) {
            accel.bias[i]    = 0.0;
            accel.gravity[i] = 0.0;
        }
        for(int i = 0; i < 9; ++i) {
            accel.temp_slope[i] = 0.0;
        }

        GyroIntrinsics &gyro = result_.gyro;
        gyro.noise_density  = 0.0;
        gyro.random_walk    = 0.0;
        gyro.reference_temp = 0.0;
        for(int i = 0; i < 3; ++i) {
            gyro.bias[i] = 0.0;
        }
        for(int i = 0; i < 9; ++i) {
            gyro.temp_slope[i] = 0.0;
        }

        MagIntrinsics &mag = result_.mag;
        mag.reference_temp = 0.0;
        for(int i = 0; i < 9; ++i) {
            mag.temp_slope[i]   = 0.0;
            mag.misalignment[i] = 0.0;
            mag.soft_iron[i]    = 0.0;
        }
        for(int i = 0; i < 3; ++i) {
            mag.scale[i]     = 0.0;
            mag.hard_iron[i] = 0.0;
        }
        result_.has_mag = false;  // no magnetometer section in params_imu.yaml yet

        identity9(mAcc_);
        identity9(mGyr_);
        identity9(cGyroI_);
        for(int i = 0; i < 9; ++i) {
            result_.accel.scale_misalignment[i] = mAcc_[i];
            result_.gyro.scale_misalignment[i]  = mGyr_[i];
        }

        inImu0_       = false;
        imu0Indent_   = 0;
        blockIndent_  = 0;
        matrixRow_    = 0;
        matrixTarget_ = nullptr;
        pendingRow_   = -1;
        blockName_.clear();
        matrixKey_.clear();
        pendingValue_.clear();
        pendingKey_.clear();
    }

    // ------------------------------------------------------------------
    // Line dispatch
    // ------------------------------------------------------------------

    void parseLine(const std::string &line) {
        std::string stripped = yamlStrip(line);
        const size_t indent  = indentOf(line);

        if(yamlIsSequenceItem(stripped)) {
            if(inImu0_) {
                handleSequenceItem(yamlStrip(stripped.substr(1)));
            }
            return;
        }

        size_t colonPos = stripped.find(':');
        if(colonPos == std::string::npos) {
            return;  // "%YAML:1.0" has a colon; "---" does not - skipped here
        }

        std::string key = yamlStrip(stripped.substr(0, colonPos));
        std::string value;
        if(colonPos + 1 < stripped.size()) {
            value = yamlStrip(stripped.substr(colonPos + 1));
        }

        if(key == "imu0") {
            inImu0_       = true;
            imu0Indent_   = indent;
            blockName_.clear();
            matrixKey_.clear();
            matrixTarget_ = nullptr;
            matrixRow_    = 0;
            return;
        }

        // A key at or above the imu0 level ends the imu0 subtree (in params_imu.yaml
        // that is the top-level `gravity:` line that follows the block).
        if(inImu0_ && indent <= imu0Indent_) {
            inImu0_ = false;
            blockName_.clear();
            matrixKey_.clear();
            matrixTarget_ = nullptr;
        }

        if(!inImu0_) {
            // `gravity` is the UNIFIED WORLD-FRAME vector (calibration-board plane 0)
            // and only exists at the top level: an `imu0.gravity` is in the IMU frame
            // and must never be mistaken for it, hence the check happens only here,
            // outside the imu0 subtree.
            if(key == "gravity") {
                handleInlineValue(key, value);
            }
            return;  // cam0 / other top-level sections are not modeled here
        }

        handleImu0Key(key, value, indent);
    }

    void handleImu0Key(const std::string &key, const std::string &value, size_t indent) {
        // A key line never belongs to a `- [ ... ]` matrix block: matrix content is
        // made of sequence items, routed by handleSequenceItem().
        matrixKey_.clear();
        matrixTarget_ = nullptr;
        matrixRow_    = 0;

        // A noise sub-block is closed by a DEDENT (a key at or left of the sub-block's
        // indent) or by a new known sub-block below - never by an unrecognised key
        // inside it. Otherwise a key a future file adds between two recognised keys
        // would silently drop everything after it.
        if((blockName_ == "accelerometer" || blockName_ == "gyroscope") && indent > blockIndent_) {
            const bool isAccel = (blockName_ == "accelerometer");
            if(isNoiseKey(key)) {
                noiseSlot(isAccel, key) = std::stod(value);
                return;
            }
            if(key == "temp_slope") {
                startMatrix(key, isAccel ? result_.accel.temp_slope : result_.gyro.temp_slope, value);
                return;
            }
            return;  // unknown key inside the sub-block: ignored here, preserved by parseImuYaml()
        }

        blockName_.clear();  // any key at the sub-block's own indent ends the noise sub-block

        if(key == "accelerometer" || key == "gyroscope") {
            blockName_   = key;
            blockIndent_ = indent;
            return;
        }

        if(key == "AccBias" || key == "GyrBias") {
            handleInlineValue(key, value);
            return;
        }

        if(key == "M_acc" || key == "M_gyr" || key == "C_gyro_i") {
            startMatrix(key, matrixTargetFor(key), value);
            return;
        }

        // imu_model / update_rate / T_imu_body / time_offset / Ma_gyr plus any key a
        // future file adds: not modeled here, preserved verbatim by parseImuYaml().
    }

    /// Reference to the noise parameter `key` of the accelerometer (`isAccel == true`)
    /// or gyroscope intrinsics; only called for isNoiseKey() keys.
    double &noiseSlot(bool isAccel, const std::string &key) {
        if(isAccel) {
            if(key == "noise_density") return result_.accel.noise_density;
            if(key == "random_walk") return result_.accel.random_walk;
            return result_.accel.reference_temp;
        }
        if(key == "noise_density") return result_.gyro.noise_density;
        if(key == "random_walk") return result_.gyro.random_walk;
        return result_.gyro.reference_temp;
    }

    /// Destination of a modeled top-level 3x3 key, or nullptr when it is one of the
    /// sub-block scoped keys (temp_slope, resolved from the enclosing block).
    double *matrixTargetFor(const std::string &key) {
        if(key == "M_acc") return mAcc_;
        if(key == "M_gyr") return mGyr_;
        if(key == "C_gyro_i") return cGyroI_;
        return nullptr;
    }

    static bool isMatrixKey(const std::string &key) {
        return key == "M_acc" || key == "M_gyr" || key == "C_gyro_i" || key == "temp_slope";
    }

    /// Begin a modeled 3x3 matrix. `value` is empty when the three `- [ ... ]` rows
    /// follow, an inline sequence on this line (`[ ... 9 numbers ... ]`) or the start
    /// of an inline sequence wrapped across lines.
    void startMatrix(const std::string &key, double *target, const std::string &value) {
        matrixKey_    = key;
        matrixTarget_ = target;
        matrixRow_    = 0;
        if(value.empty()) {
            return;
        }
        if(startsBracketUnclosed(value)) {
            pendingValue_ = value;
            pendingKey_   = key;
            pendingRow_   = -1;  // the inline 9-number form, not a single row
            return;
        }
        storeInlineMatrix(key, yamlParseInlineSequence(value));
    }

    void handleSequenceItem(const std::string &content) {
        if(matrixKey_.empty() || matrixTarget_ == nullptr || matrixRow_ >= 3) {
            return;  // a row of an unmodeled matrix (T_imu_body, Ma_gyr) or a 4th row
        }
        if(startsBracketUnclosed(content)) {
            pendingValue_ = content;
            pendingKey_   = matrixKey_;
            pendingRow_   = matrixRow_;
            return;
        }
        storeMatrixRow(matrixRow_, yamlParseInlineSequence(content));
        ++matrixRow_;
    }

    /// Dispatch an inline `key: [ ... ]` value, accumulating it across lines when the
    /// closing bracket is not on the same line.
    void handleInlineValue(const std::string &key, const std::string &value) {
        if(startsBracketUnclosed(value)) {
            pendingValue_ = value;
            pendingKey_   = key;
            pendingRow_   = -1;
            return;
        }
        applyKeyValue(key, value);
    }

    void finalizePending() {
        const std::string value = pendingValue_;
        const std::string key   = pendingKey_;
        const int         row   = pendingRow_;
        pendingValue_.clear();
        pendingKey_.clear();
        pendingRow_ = -1;

        if(row >= 0) {
            storeMatrixRow(row, yamlParseInlineSequence(value));
            matrixRow_ = row + 1;
            return;
        }
        if(isMatrixKey(key)) {
            storeInlineMatrix(key, yamlParseInlineSequence(value));
            return;
        }
        applyKeyValue(key, value);
    }

    void applyKeyValue(const std::string &key, const std::string &value) {
        double *target = nullptr;
        if(key == "gravity") {
            target = result_.accel.gravity;  // unified world frame (calibration-board plane 0)
        } else if(key == "AccBias") {
            target = result_.accel.bias;
        } else if(key == "GyrBias") {
            target = result_.gyro.bias;
        }
        if(target == nullptr) {
            return;
        }
        const std::vector<double> v = yamlParseInlineSequence(value);
        if(v.size() != 3) {
            return;  // malformed sequence: keep the default, like ImuYamlParser does
        }
        for(int i = 0; i < 3; ++i) {
            target[i] = v[i];
        }
    }

    /// Store the inline `key: [ 9 numbers ]` spelling of a modeled 3x3, rejected as a
    /// whole (keeping no partial matrix) when it does not carry exactly 9 numbers.
    void storeInlineMatrix(const std::string &key, const std::vector<double> &v) {
        if(v.size() != 9) {
            throwMalformedMatrix(key, "expected 9 numbers, got " + std::to_string(v.size()));
        }
        if(matrixTarget_ == nullptr) {
            return;
        }
        for(int i = 0; i < 9; ++i) {
            matrixTarget_[i] = v[i];
        }
    }

    /// Store one `- [ ... ]` row of a modeled 3x3 matrix, flattened row-major. A row
    /// that is not exactly 3 numbers rejects the whole matrix (see the class comment).
    void storeMatrixRow(int row, const std::vector<double> &v) {
        if(matrixTarget_ == nullptr || row < 0 || row >= 3) {
            return;
        }
        if(v.size() != 3) {
            throwMalformedMatrix(matrixKey_, "row " + std::to_string(row + 1) + " has " + std::to_string(v.size())
                                                 + " numbers, expected 3");
        }
        for(int c = 0; c < 3; ++c) {
            matrixTarget_[row * 3 + c] = v[c];
        }
    }

    static void throwMalformedMatrix(const std::string &key, const std::string &detail) {
        throw std::runtime_error(key + ": malformed 3x3 matrix (" + detail + ")");
    }
};

}  // namespace internal
}  // namespace camadapt

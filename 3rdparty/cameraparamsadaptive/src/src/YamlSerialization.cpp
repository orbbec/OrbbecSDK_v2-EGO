/**
 * @file YamlSerialization.cpp
 * @brief Implementation of YAML parsing and serialization for params_cam.yaml / params_imu.yaml.
 */

#include "camadapt/YamlSerialization.h"

#include "YamlParser.hpp"
#include "camadapt/FileIo.h"
#include "camadapt/ParamTransform.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace camadapt {

// ============================================================================
// YAML Parsing
// ============================================================================

CalibrationData parseParamsYaml(const std::string &yamlContent) {
    internal::YamlParser parser;
    return parser.parse(yamlContent);
}

CameraImuParams parseImuYaml(const std::string &yamlContent) {
    internal::ImuYamlParser parser;
    return parser.parse(yamlContent);
}

ImuIntrinsics parseImuIntrinsics(const std::string &yamlContent) {
    internal::ImuIntrinsicsParser parser;
    return parser.parse(yamlContent);
}

// ============================================================================
// YAML Serialization
// ============================================================================

namespace {

/**
 * @brief Write a 3x3 rotation matrix row as YAML inline sequence.
 */
void writeRotationRow(std::ostringstream &oss, int indent, const Mat33 &R, int row) {
    oss << std::string(indent, ' ') << "- [" << std::fixed << std::setprecision(6) << R.m[row][0] << ", "
        << R.m[row][1] << ", " << R.m[row][2] << "]\n";
}

/**
 * @brief Quote a YAML scalar only when it cannot be written as a plain scalar.
 *
 * Identifiers such as cam_0 / IR_L / KB are emitted bare, matching the algorithm
 * team's hand-written params_cam.yaml. Values that would otherwise change
 * meaning stay quoted so the field is still a string on re-parse (design doc
 * §11.2: Name is a string type): numeric-looking strings (a camera named "0"),
 * YAML 1.1 booleans/null, empty strings, and any string containing spaces or
 * other special characters.
 */
std::string yamlScalar(const std::string &s) {
    bool plain = !s.empty();
    for(size_t i = 0; plain && i < s.size(); ++i) {
        const unsigned char c          = static_cast<unsigned char>(s[i]);
        const bool          wordChar   = std::isalnum(c) != 0 || c == '_' || c == '-' || c == '.';
        // A leading digit, '-' or '.' could start a number, so keep those quoted.
        plain = wordChar && !(i == 0 && (std::isdigit(c) != 0 || c == '-' || c == '.'));
    }
    if(plain) {
        std::string lower = s;
        for(char &c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        static const char *const reserved[] = {"true", "false", "null", "yes", "no", "on", "off", "none", "nan", "inf"};
        for(const char *word : reserved) {
            if(lower == word) {
                plain = false;
                break;
            }
        }
    }
    if(plain) {
        return s;
    }
    std::string quoted = "\"";
    for(char c : s) {
        if(c == '"' || c == '\\') {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

/**
 * @brief Write a CameraParams to YAML.
 */
void writeCameraParams(std::ostringstream &oss, const CameraParams &cam, int indent) {
    std::string in(indent, ' ');
    std::string in2(indent + 2, ' ');
    std::string in4(indent + 4, ' ');

    oss << in << "- id: " << yamlScalar(cam.camera_id) << "\n";
    oss << in2 << "name: " << yamlScalar(cam.camera_name) << "\n";
    oss << in2 << "distortion_model: " << yamlScalar(cam.distortion.model) << "\n";
    oss << in2 << "image_width: " << cam.image_size.width << "\n";
    oss << in2 << "image_height: " << cam.image_size.height << "\n";

    // Intrinsics (fixed 9 decimals for stable round-trips)
    oss << in2 << "intrinsics:\n";
    oss << in4 << "fx: " << std::fixed << std::setprecision(9) << cam.intrinsics.fx << "\n";
    oss << in4 << "fy: " << cam.intrinsics.fy << "\n";
    oss << in4 << "cx: " << cam.intrinsics.cx << "\n";
    oss << in4 << "cy: " << cam.intrinsics.cy << "\n";

    // Distortion
    oss << in2 << "distortion:\n";
    oss << in4 << "k1: " << std::fixed << std::setprecision(9) << cam.distortion.k1 << "\n";
    oss << in4 << "k2: " << cam.distortion.k2 << "\n";
    oss << in4 << "k3: " << cam.distortion.k3 << "\n";
    oss << in4 << "k4: " << cam.distortion.k4 << "\n";
    oss << in4 << "k5: " << cam.distortion.k5 << "\n";
    oss << in4 << "k6: " << cam.distortion.k6 << "\n";
    oss << in4 << "p1: " << cam.distortion.p1 << "\n";
    oss << in4 << "p2: " << cam.distortion.p2 << "\n";

    // Extrinsics
    oss << in2 << "extrinsics:\n";
    oss << in4 << "rotation:\n";
    writeRotationRow(oss, indent + 6, cam.extrinsics.rotation, 0);
    writeRotationRow(oss, indent + 6, cam.extrinsics.rotation, 1);
    writeRotationRow(oss, indent + 6, cam.extrinsics.rotation, 2);
    oss << in4 << "translation: [" << std::fixed << std::setprecision(6) << cam.extrinsics.translation.x << ", "
        << cam.extrinsics.translation.y << ", " << cam.extrinsics.translation.z << "]\n";

    oss << in2 << "vcm_code: " << cam.vcm_code << "\n";
}

}  // anonymous namespace

std::string toYaml(const CameraParams &params) {
    std::ostringstream oss;
    oss << "cameras:\n";
    writeCameraParams(oss, params, 2);
    return oss.str();
}

std::string toYaml(const CalibrationData &data) {
    std::ostringstream oss;
    oss << "cameras:\n";
    for(const auto &cam : data.cameras) {
        writeCameraParams(oss, cam, 2);
        oss << "\n";
    }
    return oss.str();
}

bool saveYamlFile(const std::string &path, const CameraParams &params) {
    return writeTextFile(path, toYaml(params));
}

bool saveYamlFile(const std::string &path, const CalibrationData &data) {
    return writeTextFile(path, toYaml(data));
}

bool saveYamlFile(const std::string &path, const CameraImuParams &imu) {
    return writeTextFile(path, toYaml(imu));
}

/**
 * @brief Emit one cam0 field in its preserved source order.
 *
 * Known keys are rendered from the modeled struct fields; unknown entries are
 * raw "key: value" lines captured by the parser (design doc §5.2).
 */
static void writeCam0Field(std::ostringstream &oss, const CameraImuParams &imu, const std::string &entry) {
    if(entry == "T_cam_imu") {
        oss << "   T_cam_imu:\n";
        for(int r = 0; r < 3; ++r) {
            oss << "      - [" << std::fixed << std::setprecision(6) << imu.T_cam_imu.rotation.m[r][0] << ", "
                << imu.T_cam_imu.rotation.m[r][1] << ", " << imu.T_cam_imu.rotation.m[r][2] << ", ";
            if(r == 0) oss << imu.T_cam_imu.translation.x << "]\n";
            else if(r == 1) oss << imu.T_cam_imu.translation.y << "]\n";
            else oss << imu.T_cam_imu.translation.z << "]\n";
        }
        oss << "      - [0.0, 0.0, 0.0, 1.0]\n";
    } else if(entry == "timeshift_cam_imu") {
        oss << "   timeshift_cam_imu: " << std::fixed << std::setprecision(6) << imu.timeshift_cam_imu << "\n";
    } else if(entry == "intrinsics") {
        // params_imu.yaml order is [cx, cy, fx, fy] (OpenCV-style IMU format).
        oss << "   intrinsics: [" << std::fixed << std::setprecision(6) << imu.intrinsics.cx << ", "
            << imu.intrinsics.cy << ", " << imu.intrinsics.fx << ", " << imu.intrinsics.fy << "]\n";
    } else if(entry == "distortion_model") {
        oss << "   distortion_model: " << imu.distortion_model << "\n";
    } else if(entry == "distortion_coeffs") {
        oss << "   distortion_coeffs: [";
        for(size_t i = 0; i < imu.distortion_coeffs.size(); ++i) {
            if(i > 0) oss << ", ";
            oss << std::fixed << std::setprecision(6) << imu.distortion_coeffs[i];
        }
        oss << "]\n";
    } else if(entry == "resolution") {
        oss << "   resolution: [" << imu.resolution.width << ", " << imu.resolution.height << "]\n";
    } else {
        // Non-modeled field preserved as-is (raw "key: value" line)
        oss << "   " << entry << "\n";
    }
}

std::string toYaml(const CameraImuParams &imu) {
    std::ostringstream oss;
    oss << "cam0:\n";
    if(imu.cam0_order.empty()) {
        // Programmatically-built data without a recorded source order: standard order
        writeCam0Field(oss, imu, "T_cam_imu");
        writeCam0Field(oss, imu, "timeshift_cam_imu");
        writeCam0Field(oss, imu, "intrinsics");
        writeCam0Field(oss, imu, "distortion_model");
        writeCam0Field(oss, imu, "distortion_coeffs");
        writeCam0Field(oss, imu, "resolution");
    } else {
        for(const auto &entry : imu.cam0_order) {
            writeCam0Field(oss, imu, entry);
        }
    }
    oss << "imu0:\n";
    if(!imu.imu0_text.empty()) {
        oss << imu.imu0_text << "\n";
    }
    oss << "gravity: [";
    for(size_t i = 0; i < imu.gravity.size(); ++i) {
        if(i > 0) oss << ", ";
        oss << std::fixed << std::setprecision(6) << imu.gravity[i];
    }
    oss << "]\n";
    return oss.str();
}

CameraImuParams reoutputImuParams(const CameraImuParams &original, const CameraParams &newCam0, const ImageTransform &orientation) {
    CameraImuParams result = original;

    // cam0 intrinsics and resolution follow the freshly computed cam0 parameters
    // so they stay consistent with params_cam.yaml (design doc §5.2).
    result.intrinsics = newCam0.intrinsics;
    result.resolution = newCam0.image_size;

    // T_cam_imu' = O_3d × T_cam_imu; only the orientation (rotate/mirror/flip)
    // of the mode matters — crop/scale are image-space and do not touch 3D extrinsics.
    // transformExtrinsics() builds O_3d (Rotate → Mirror → Flip) internally.
    ImageTransform orientationOnly;
    orientationOnly.rotate = orientation.rotate;
    orientationOnly.mirror = orientation.mirror;
    orientationOnly.flip   = orientation.flip;
    result.T_cam_imu = transformExtrinsics(original.T_cam_imu, orientationOnly);

    return result;
}

}  // namespace camadapt

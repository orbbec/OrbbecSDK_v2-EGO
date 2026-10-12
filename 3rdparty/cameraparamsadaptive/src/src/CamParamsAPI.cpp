/**
 * @file CamParamsAPI.cpp
 * @brief Implementation of the high-level convenience API.
 */

#include "camadapt/CamParamsAPI.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace camadapt {

// ============================================================================
// Public API
// ============================================================================

CameraParams getCameraParams(const ConvertConfig &config, const CalibrationData &calib, const std::string &cameraId, int width, int height, double runtimeTempC) {
    // Find camera config by ID
    const CameraConfig *camConfig = nullptr;
    for(const auto &cam : config.cameras) {
        if(cam.id == cameraId) {
            camConfig = &cam;
            break;
        }
    }
    if(!camConfig) {
        std::ostringstream oss;
        oss << "Camera ID " << cameraId << " not found in convert.json";
        throw std::invalid_argument(oss.str());
    }

    // Find mode by output resolution
    const CameraMode *mode = nullptr;
    for(const auto &m : camConfig->modes) {
        if(m.output_size.width == width && m.output_size.height == height) {
            mode = &m;
            break;
        }
    }
    if(!mode) {
        std::ostringstream oss;
        oss << "Resolution " << width << "x" << height << " not found for camera ID " << cameraId;
        throw std::invalid_argument(oss.str());
    }

    // Find original calibration data by camera ID
    const CameraParams *origParams = nullptr;
    for(const auto &cam : calib.cameras) {
        if(cam.camera_id == cameraId) {
            origParams = &cam;
            break;
        }
    }
    if(!origParams) {
        std::ostringstream oss;
        oss << "Camera ID " << cameraId << " not found in params.yaml";
        throw std::invalid_argument(oss.str());
    }

    // Temperature compensation (design doc §3.7): adjust the calibration focal
    // length BEFORE the resolution pipeline so the compensation scales with the
    // mode's scale/skipping/binning factors. Disabled when runtimeTempC is NaN
    // (default) or temp_coeff == 0.
    CameraParams compensated = *origParams;
    if(camConfig->temp_coeff != 0.0 && !std::isnan(runtimeTempC)) {
        if(!std::isfinite(origParams->calibration_temperature)) {
            std::ostringstream oss;
            oss << "Camera ID " << cameraId
                << ": temp_coeff != 0 but calibration temperature is missing in params.yaml";
            throw std::invalid_argument(oss.str());
        }
        compensated.intrinsics = compensateTemperature(origParams->intrinsics,
            origParams->calibration_temperature, runtimeTempC, camConfig->temp_coeff);
    }

    // Re-anchor the calibration to the full-sensor frame (design doc §4.10): the
    // calibration may have been done on a centred sub-window of the sensor, while
    // every crop in convert.json is expressed in sensor coordinates. No-op when the
    // calibration already used the full sensor.
    compensated = anchorCalibrationToSensor(compensated, camConfig->sensor_size);

    // Compute transformed parameters
    return computeCameraParams(compensated, mode->transform);
}

CalibrationData getCameraParams(const ConvertConfig &config, const CalibrationData &calib, int width, int height, double runtimeTempC) {
    CalibrationData all;
    for(const auto &cam : config.cameras) {
        all.cameras.push_back(getCameraParams(config, calib, cam.id, width, height, runtimeTempC));
    }
    return all;
}

std::vector<Size> listSupportedResolutions(const ConvertConfig &config, const std::string &cameraId) {
    // Find camera config by ID
    const CameraConfig *camConfig = nullptr;
    for(const auto &cam : config.cameras) {
        if(cam.id == cameraId) {
            camConfig = &cam;
            break;
        }
    }
    if(!camConfig) {
        std::ostringstream oss;
        oss << "Camera ID " << cameraId << " not found in convert.json";
        throw std::invalid_argument(oss.str());
    }

    std::vector<Size> resolutions;
    for(const auto &mode : camConfig->modes) {
        resolutions.push_back(mode.output_size);
    }
    return resolutions;
}

std::vector<std::string> listCamerasSupportingResolution(const ConvertConfig &config, int width, int height) {
    // Pure query: a camera that does not offer the resolution is simply not listed, so a
    // device whose cameras expose different mode sets (§1.2 requirement 2) does not have to
    // go through an exception to find out which cameras it can ask for.
    std::vector<std::string> ids;
    for(const auto &cam : config.cameras) {
        for(const auto &mode : cam.modes) {
            if(mode.output_size.width == width && mode.output_size.height == height) {
                ids.push_back(cam.id);
                break;
            }
        }
    }
    return ids;
}

std::string getAlgorithmVersion() {
    return "2.3.1";
}

}  // namespace camadapt
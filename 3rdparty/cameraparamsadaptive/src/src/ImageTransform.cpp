/**
 * @file ImageTransform.cpp
 * @brief Image-geometry helpers shared by firmware and the SDK.
 *
 * Currently: re-expressing a crop origin in the sensor read-out frame, so that a
 * module whose read-out is mirrored/flipped still crops the intended region
 * (design doc §4.9).
 */

#include "camadapt/ImageTransform.h"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace camadapt {

namespace {

/// Largest multiple of kCropAlignment that is <= v (proper floor, negatives included).
int alignDown(int v) {
    int remainder = v % kCropAlignment;
    if(remainder < 0) {
        remainder += kCropAlignment;
    }
    return v - remainder;
}

}  // namespace

Rect mapCropToReadoutFrame(const Rect &crop, const Size &sensorSize, const ReadoutOrientation &readout) {
    Rect result = crop;

    int x = crop.x;
    int y = crop.y;

    // Mirroring/flipping the read-out re-expresses the origin about the sensor
    // centre (design doc §4.9 约定 2). Width/height do not change: the same
    // physical region is selected, only its position in the read-out frame differs.
    if(readout.mirror) {
        x = sensorSize.width - crop.x - crop.width;
    }
    if(readout.flip) {
        y = sensorSize.height - crop.y - crop.height;
    }

    // Defensive: never hand firmware an off-grid origin. When (sensor − crop) is
    // aligned — which validateConvertJson() requires — the mirrored origin is
    // already aligned and this floor changes nothing.
    result.x = alignDown(x);
    result.y = alignDown(y);
    return result;
}

void ReadoutStateConfig::set(const std::string &cameraId, bool mirror, bool flip) {
    for(auto &entry : cameras) {
        if(entry.camera_id == cameraId) {
            entry.orientation = ReadoutOrientation(mirror, flip);  // replace, do not add a second entry
            return;
        }
    }

    CameraReadoutState entry;
    entry.camera_id   = cameraId;
    entry.orientation = ReadoutOrientation(mirror, flip);
    cameras.push_back(entry);
}

void SensorSizeConfig::set(const std::string &cameraId, int width, int height) {
    for(auto &entry : cameras) {
        if(entry.camera_id == cameraId) {
            entry.sensor_size = Size{width, height};  // replace, do not add a second entry
            return;
        }
    }

    CameraSensorSize entry;
    entry.camera_id   = cameraId;
    entry.sensor_size = Size{width, height};
    cameras.push_back(entry);
}

void ModuleIdConfig::set(const std::string &cameraId, const std::string &module) {
    for(auto &entry : cameras) {
        if(entry.camera_id == cameraId) {
            entry.module = module;  // replace, do not add a second entry
            return;
        }
    }

    CameraModuleId entry;
    entry.camera_id = cameraId;
    entry.module    = module;
    cameras.push_back(entry);
}

size_t applyReadoutStates(ConvertConfig &config, const ReadoutStateConfig &actual) {
    // Validate everything BEFORE touching @p config, so a bad hand-in cannot leave the
    // config half-updated.
    for(size_t i = 0; i < actual.cameras.size(); ++i) {
        for(size_t j = i + 1; j < actual.cameras.size(); ++j) {
            if(actual.cameras[i].camera_id == actual.cameras[j].camera_id) {
                throw std::invalid_argument("applyReadoutStates: camera '" + actual.cameras[i].camera_id
                                            + "' is configured more than once");
            }
        }
    }

    for(const auto &entry : actual.cameras) {
        bool known = false;
        for(const auto &cam : config.cameras) {
            if(cam.id == entry.camera_id) {
                known = true;
                break;
            }
        }
        if(!known) {
            throw std::invalid_argument("applyReadoutStates: unknown camera '" + entry.camera_id + "'");
        }
    }

    for(const auto &cam : config.cameras) {
        bool configured = false;
        for(const auto &entry : actual.cameras) {
            if(entry.camera_id == cam.id) {
                configured = true;
                break;
            }
        }
        if(!configured) {
            throw std::invalid_argument("applyReadoutStates: camera '" + cam.id
                                        + "' has no read-out state (every camera must be configured)");
        }
    }

    size_t changed = 0;
    for(auto &cam : config.cameras) {
        for(const auto &entry : actual.cameras) {
            if(entry.camera_id != cam.id) {
                continue;
            }
            if(cam.readout_orientation.mirror != entry.orientation.mirror
               || cam.readout_orientation.flip != entry.orientation.flip) {
                ++changed;
            }
            cam.readout_orientation = entry.orientation;  // absolute assignment, never a toggle
            break;
        }
    }
    return changed;
}

}  // namespace camadapt

/**
 * @file ParamTransform.cpp
 * @brief Implementation of the parameter transformation engine.
 *
 * Transform order: Crop → Scale → Rotate → Mirror → Flip
 */

#include "camadapt/ParamTransform.h"

#include <cmath>
#include <stdexcept>
#include <string>

namespace camadapt {

// ============================================================================
// Private Helpers
// ============================================================================

/**
 * @brief Compute effective scale factor combining scale, skipping, and binning.
 */
static Vec2 computeEffectiveScale(const ImageTransform &transform) {
    double sx = transform.scale.x;
    double sy = transform.scale.y;
    if(transform.skipping > 1) {
        double s = static_cast<double>(transform.skipping);
        sx *= s;
        sy *= s;
    }
    if(transform.binning > 1) {
        double b = static_cast<double>(transform.binning);
        sx *= b;
        sy *= b;
    }
    return {sx, sy};
}

/**
 * @brief Validate the image transform for correctness.
 */
static void validateTransform(const ImageTransform &transform, const Size &origSize) {
    if(transform.crop.x < 0 || transform.crop.y < 0) {
        throw std::invalid_argument("Crop origin must be non-negative");
    }
    if(transform.crop.width <= 0 || transform.crop.height <= 0) {
        throw std::invalid_argument("Crop dimensions must be positive");
    }
    // Compare in 64-bit, NOT as crop_x + crop_width: the int sum wraps for large
    // crop_x/crop_width (e.g. crop_x = 2147483646, crop_width = 2 on a 1600-wide image
    // wraps to INT_MIN and "passes"), letting an out-of-bounds crop through. Widening
    // cannot overflow for any int inputs, and crop_x >= 0 / crop_width > 0 were already
    // established above, so the rejection semantics are otherwise unchanged.
    const long long cropRight  = static_cast<long long>(transform.crop.x) + transform.crop.width;
    const long long cropBottom = static_cast<long long>(transform.crop.y) + transform.crop.height;
    if(cropRight > origSize.width || cropBottom > origSize.height) {
        throw std::invalid_argument("Crop region exceeds original image bounds");
    }
    if(transform.scale.x <= 0.0 || transform.scale.y <= 0.0) {
        throw std::invalid_argument("Scale factors must be positive");
    }
    int rotVal = static_cast<int>(transform.rotate);
    if(rotVal != 0 && rotVal != 90 && rotVal != 180 && rotVal != 270) {
        throw std::invalid_argument("Rotation must be one of {0, 90, 180, 270}");
    }
}

/**
 * @brief Build the 3D orthogonal transform matrix O_3d from image operations.
 *
 * Accumulates rotation, mirror, and flip into a single 3x3 matrix.
 * Order: Rotate → Mirror → Flip (left-multiplied).
 */
static Mat33 buildO3d(const ImageTransform &transform) {
    // Start with identity
    Mat33 O;
    for(int r = 0; r < 3; ++r) {
        for(int c = 0; c < 3; ++c) {
            O.m[r][c] = (r == c) ? 1.0 : 0.0;
        }
    }

    // Step 1: Rotation
    switch(transform.rotate) {
    case Rotation::DEG_0:
        // Identity: no change
        break;
    case Rotation::DEG_90:
        // [[0, -1, 0], [1, 0, 0], [0, 0, 1]]
        O.m[0][0] = 0.0; O.m[0][1] = -1.0; O.m[0][2] = 0.0;
        O.m[1][0] = 1.0; O.m[1][1] = 0.0;  O.m[1][2] = 0.0;
        O.m[2][0] = 0.0; O.m[2][1] = 0.0;  O.m[2][2] = 1.0;
        break;
    case Rotation::DEG_180:
        // [[-1, 0, 0], [0, -1, 0], [0, 0, 1]]
        O.m[0][0] = -1.0; O.m[0][1] = 0.0;  O.m[0][2] = 0.0;
        O.m[1][0] = 0.0;  O.m[1][1] = -1.0; O.m[1][2] = 0.0;
        O.m[2][0] = 0.0;  O.m[2][1] = 0.0;  O.m[2][2] = 1.0;
        break;
    case Rotation::DEG_270:
        // [[0, 1, 0], [-1, 0, 0], [0, 0, 1]]
        O.m[0][0] = 0.0;  O.m[0][1] = 1.0; O.m[0][2] = 0.0;
        O.m[1][0] = -1.0; O.m[1][1] = 0.0; O.m[1][2] = 0.0;
        O.m[2][0] = 0.0;  O.m[2][1] = 0.0; O.m[2][2] = 1.0;
        break;
    }

    // Step 2: Mirror (left-multiply by diag(-1, 1, 1))
    if(transform.mirror) {
        Mat33 mirrorMat;
        for(int r = 0; r < 3; ++r) {
            for(int c = 0; c < 3; ++c) {
                mirrorMat.m[r][c] = (r == c) ? 1.0 : 0.0;
            }
        }
        mirrorMat.m[0][0] = -1.0;

        // mirrorMat * O
        Mat33 result;
        for(int r = 0; r < 3; ++r) {
            for(int c = 0; c < 3; ++c) {
                result.m[r][c] = 0.0;
                for(int k = 0; k < 3; ++k) {
                    result.m[r][c] += mirrorMat.m[r][k] * O.m[k][c];
                }
            }
        }
        O = result;
    }

    // Step 3: Flip (left-multiply by diag(1, -1, 1))
    if(transform.flip) {
        Mat33 flipMat;
        for(int r = 0; r < 3; ++r) {
            for(int c = 0; c < 3; ++c) {
                flipMat.m[r][c] = (r == c) ? 1.0 : 0.0;
            }
        }
        flipMat.m[1][1] = -1.0;

        // flipMat * O
        Mat33 result;
        for(int r = 0; r < 3; ++r) {
            for(int c = 0; c < 3; ++c) {
                result.m[r][c] = 0.0;
                for(int k = 0; k < 3; ++k) {
                    result.m[r][c] += flipMat.m[r][k] * O.m[k][c];
                }
            }
        }
        O = result;
    }

    return O;
}

/**
 * @brief Multiply two 3x3 matrices: C = A * B.
 */
static Mat33 mat33Mul(const Mat33 &A, const Mat33 &B) {
    Mat33 C;
    for(int r = 0; r < 3; ++r) {
        for(int c = 0; c < 3; ++c) {
            C.m[r][c] = 0.0;
            for(int k = 0; k < 3; ++k) {
                C.m[r][c] += A.m[r][k] * B.m[k][c];
            }
        }
    }
    return C;
}

/**
 * @brief Multiply 3x3 matrix by 3x1 vector: v' = M * v.
 */
static Vec3 mat33Vec3Mul(const Mat33 &M, const Vec3 &v) {
    Vec3 result;
    result.x = M.m[0][0] * v.x + M.m[0][1] * v.y + M.m[0][2] * v.z;
    result.y = M.m[1][0] * v.x + M.m[1][1] * v.y + M.m[1][2] * v.z;
    result.z = M.m[2][0] * v.x + M.m[2][1] * v.y + M.m[2][2] * v.z;
    return result;
}

// ============================================================================
// Public API
// ============================================================================

Intrinsics compensateTemperature(const Intrinsics &origK, double calibTempC, double runtimeTempC, double tempCoeff) {
    Intrinsics K = origK;
    if(tempCoeff == 0.0 || std::isnan(runtimeTempC)) {
        return K;
    }
    // A missing calibration temperature cannot be compensated for: dT would be NaN and
    // would silently poison fx/fy. Same policy as the query path (CamParamsAPI throws for
    // "temp_coeff != 0 but the camera has no calibration temperature", §3.7).
    if(!std::isfinite(calibTempC)) {
        throw std::invalid_argument(
            "compensateTemperature: calibration temperature is missing/invalid while temp_coeff != 0 (§3.7)");
    }
    double dT = runtimeTempC - calibTempC;
    if(dT == 0.0) {
        return K;
    }
    K.fx += dT * tempCoeff;
    K.fy += dT * tempCoeff;
    return K;
}

Intrinsics transformIntrinsics(const Intrinsics &origK, const Size &origSize, const ImageTransform &transform) {
    Intrinsics K = origK;
    double    W = static_cast<double>(transform.crop.width);
    double    H = static_cast<double>(transform.crop.height);

    // Step 1: Crop
    K.cx -= static_cast<double>(transform.crop.x);
    K.cy -= static_cast<double>(transform.crop.y);

    // Step 2: Scale (including skipping and binning)
    Vec2 effScale = computeEffectiveScale(transform);
    K.fx *= effScale.x;
    K.fy *= effScale.y;
    K.cx *= effScale.x;
    K.cy *= effScale.y;
    W *= effScale.x;
    H *= effScale.y;

    // Step 3: Rotate
    switch(transform.rotate) {
    case Rotation::DEG_0:
        // No change
        break;
    case Rotation::DEG_90: {
        // swap(fx, fy), cx' = H - cy, cy' = cx, swap(W, H)
        double newFx = K.fy;
        double newFy = K.fx;
        double newCx = H - K.cy;
        double newCy = K.cx;
        K.fx        = newFx;
        K.fy        = newFy;
        K.cx        = newCx;
        K.cy        = newCy;
        double tmp  = W;
        W           = H;
        H           = tmp;
        break;
    }
    case Rotation::DEG_180:
        // cx = W - cx, cy = H - cy
        K.cx = W - K.cx;
        K.cy = H - K.cy;
        break;
    case Rotation::DEG_270: {
        // swap(fx, fy), cx' = cy, cy' = W - cx, swap(W, H)
        double newFx = K.fy;
        double newFy = K.fx;
        double newCx = K.cy;
        double newCy = W - K.cx;
        K.fx        = newFx;
        K.fy        = newFy;
        K.cx        = newCx;
        K.cy        = newCy;
        double tmp  = W;
        W           = H;
        H           = tmp;
        break;
    }
    }

    // Step 4: Mirror
    if(transform.mirror) {
        K.cx = W - K.cx;
    }

    // Step 5: Flip
    if(transform.flip) {
        K.cy = H - K.cy;
    }

    return K;
}

Distortion transformDistortion(const Distortion &origD, const ImageTransform &transform) {
    Distortion D = origD;

    // k1-k6 are invariant under orthogonal transforms
    // p1, p2 transform as a 2D vector

    double p1 = origD.p1;
    double p2 = origD.p2;
    double p1New, p2New;

    // Step 1: Rotate
    switch(transform.rotate) {
    case Rotation::DEG_0:
        p1New = p1;
        p2New = p2;
        break;
    case Rotation::DEG_90:
        p1New = p2;
        p2New = -p1;
        break;
    case Rotation::DEG_180:
        p1New = -p1;
        p2New = -p2;
        break;
    case Rotation::DEG_270:
        p1New = -p2;
        p2New = p1;
        break;
    default:
        p1New = p1;
        p2New = p2;
        break;
    }
    p1 = p1New;
    p2 = p2New;

    // Step 2: Mirror
    if(transform.mirror) {
        p2 = -p2;
    }

    // Step 3: Flip
    if(transform.flip) {
        p1 = -p1;
    }

    D.p1 = p1;
    D.p2 = p2;

    return D;
}

Extrinsics transformExtrinsics(const Extrinsics &origE, const ImageTransform &transform) {
    Extrinsics E;

    // Build the 3D orthogonal transform matrix
    Mat33 O3d = buildO3d(transform);

    // R' = O_3d * R
    E.rotation = mat33Mul(O3d, origE.rotation);

    // t' = O_3d * t
    E.translation = mat33Vec3Mul(O3d, origE.translation);

    return E;
}

CameraParams anchorCalibrationToSensor(const CameraParams &calib, const Size &sensorSize) {
    const int dw = sensorSize.width - calib.image_size.width;
    const int dh = sensorSize.height - calib.image_size.height;

    if(dw < 0 || dh < 0) {
        throw std::invalid_argument("anchorCalibrationToSensor: calibration image "
                                    + std::to_string(calib.image_size.width) + "x"
                                    + std::to_string(calib.image_size.height) + " is larger than the sensor "
                                    + std::to_string(sensorSize.width) + "x" + std::to_string(sensorSize.height));
    }
    if(dw % 2 != 0 || dh % 2 != 0) {
        throw std::invalid_argument(
            "anchorCalibrationToSensor: the sensor-to-calibration difference must be even on both axes "
            "(the calibration window is assumed to be centred), got "
            + std::to_string(dw) + "x" + std::to_string(dh));
    }
    if(dw == 0 && dh == 0) {
        return calib;  // already calibrated on the full sensor
    }

    CameraParams result = calib;
    result.intrinsics.cx += static_cast<double>(dw / 2);
    result.intrinsics.cy += static_cast<double>(dh / 2);
    result.image_size = sensorSize;
    return result;
}

CameraParams computeCameraParams(const CameraParams &original, const ImageTransform &transform) {
    // Validate the transform
    validateTransform(transform, original.image_size);

    CameraParams result;

    // Copy identity fields
    result.camera_id   = original.camera_id;
    result.camera_name = original.camera_name;
    result.vcm_code    = original.vcm_code;
    // The calibration temperature is a property of the calibration itself (from
    // params_cam.yaml environment.temperature, §3.7), not of the resolution mode, so it
    // is carried through unchanged. Dropping it made the temperature compensation of
    // every derived mode throw ("calibration temperature is missing") / read back as NaN.
    result.calibration_temperature = original.calibration_temperature;

    // Compute output image size
    // Start from crop size, apply scale, then handle rotation
    Vec2 effScale = computeEffectiveScale(transform);
    int  outW     = static_cast<int>(std::round(transform.crop.width * effScale.x));
    int  outH     = static_cast<int>(std::round(transform.crop.height * effScale.y));

    // Swap for 90°/270° rotation
    if(transform.rotate == Rotation::DEG_90 || transform.rotate == Rotation::DEG_270) {
        std::swap(outW, outH);
    }

    result.image_size.width  = outW;
    result.image_size.height = outH;

    // Transform each component
    result.intrinsics = transformIntrinsics(original.intrinsics, original.image_size, transform);
    result.distortion = transformDistortion(original.distortion, transform);
    result.extrinsics = transformExtrinsics(original.extrinsics, transform);

    return result;
}

}  // namespace camadapt
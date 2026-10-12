/**
 * @file ParamTransform.h
 * @brief Parameter transformation engine — computes camera parameters at target resolutions.
 *
 * Transform order: Crop → Scale → Rotate → Mirror → Flip
 */

#pragma once

#include "ImageTransform.h"
#include "Types.h"

namespace camadapt {

/**
 * @brief Re-express a sub-window calibration in full-sensor coordinates.
 *
 * A camera may have been calibrated on a **centred sub-window** of its sensor
 * (`params_cam.yaml` image_size < sensor size), with no scaling — that is how the
 * early TN2608-A-250201157 units were calibrated at 1600x1200 on a 1920x1200 sensor
 * (design doc §4.10). Every crop in convert.json is expressed in **full-sensor**
 * coordinates, so before those crops can be applied the calibration has to be moved
 * into the sensor frame: the principal point is shifted by the centring offset
 *
 *     o = ((sensor_w − calib_w) / 2, (sensor_h − calib_h) / 2)
 *
 * and the image size is set to the sensor size. Intrinsics, distortion and extrinsics
 * are otherwise untouched (the sub-window has no scaling, and 3D extrinsics do not
 * depend on the image window). When the calibration already used the full sensor —
 * the normal case — the offset is zero and this is a no-op.
 *
 * @param calib       Calibration of one camera (from params_cam.yaml).
 * @param sensorSize  Sensor native resolution.
 * @return The calibration re-expressed in the sensor frame.
 * @throws std::invalid_argument when the calibration image is larger than the sensor
 *         or the per-axis difference is odd (no integer centred offset exists).
 */
CameraParams anchorCalibrationToSensor(const CameraParams &calib, const Size &sensorSize);

/**
 * @brief Compute camera parameters at the target resolution given an image transform.
 *
 * This is the main entry point for the transform engine. It validates the transform,
 * then applies the pipeline: Crop → Scale → Rotate → Mirror → Flip to intrinsics,
 * distortion, and extrinsics.
 *
 * @param original  Original calibration parameters (at native resolution).
 * @param transform Image transform descriptor.
 * @return CameraParams at the target resolution.
 * @throws std::invalid_argument if the transform is invalid.
 */
CameraParams computeCameraParams(const CameraParams &original, const ImageTransform &transform);

/**
 * @brief Transform camera intrinsics through the image pipeline.
 *
 * @note The principal point is placed using the *unrounded* scaled size
 *       (crop_width × eff_scale_x / crop_height × eff_scale_y), while the emitted
 *       image size is round() of the same product. A mirrored or rotated mode can
 *       therefore be asymmetric by up to 0.5 px (design doc §9.6).
 *
 * @param origK       Original intrinsics.
 * @param origSize    Original image size (width, height).
 * @param transform   Image transform descriptor.
 * @return Transformed intrinsics.
 */
Intrinsics transformIntrinsics(const Intrinsics &origK, const Size &origSize, const ImageTransform &transform);

/**
 * @brief Transform distortion coefficients through the image pipeline.
 *
 * Radial distortion (k1-k6) is invariant under orthogonal transforms.
 * Tangential distortion (p1, p2) rotates and reflects as a 2D vector.
 *
 * @param origD      Original distortion coefficients.
 * @param transform  Image transform descriptor.
 * @return Transformed distortion coefficients.
 */
Distortion transformDistortion(const Distortion &origD, const ImageTransform &transform);

/**
 * @brief Compensate camera intrinsics focal length for temperature drift.
 *
 * Linear model (design doc §3.7):
 *   F' = F + (runtimeTempC - calibTempC) * tempCoeff
 * applied to fx and fy (px/°C at the native resolution). Call this on the
 * calibration intrinsics BEFORE the resolution pipeline so the compensation
 * scales together with the mode's scale/skipping/binning factors.
 *
 * Returns the input unchanged when tempCoeff == 0, runtimeTempC is NaN
 * (temperature compensation disabled), or ΔT == 0.
 *
 * @param origK        Original (calibration) intrinsics.
 * @param calibTempC   Calibration temperature in °C.
 * @param runtimeTempC Runtime temperature in °C (NaN = no compensation).
 * @param tempCoeff    Temperature compensation coefficient in px/°C.
 * @return Compensated intrinsics.
 * @throws std::invalid_argument when compensation is actually requested
 *         (tempCoeff != 0 and runtimeTempC is not NaN) but @p calibTempC is not finite
 *         — i.e. the calibration file provided no temperature for this camera (§3.7).
 *         Compensating with ΔT = NaN would silently turn fx/fy into NaN.
 */
Intrinsics compensateTemperature(const Intrinsics &origK, double calibTempC, double runtimeTempC, double tempCoeff);

/**
 * @brief Transform camera extrinsics through the image pipeline.
 *
 * Applies the 3D orthogonal transform O_3d to both rotation and translation:
 *   R' = O_3d * R
 *   t' = O_3d * t
 *
 * @param origE      Original extrinsics.
 * @param transform  Image transform descriptor.
 * @return Transformed extrinsics.
 */
Extrinsics transformExtrinsics(const Extrinsics &origE, const ImageTransform &transform);

}  // namespace camadapt
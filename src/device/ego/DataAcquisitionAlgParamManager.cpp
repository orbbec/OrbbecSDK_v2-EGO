// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "DataAcquisitionAlgParamManager.hpp"

#include "property/InternalProperty.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "publicfilters/IMUCorrector.hpp"
#include "exception/ObException.hpp"
#include "utils/FileUtils.hpp"

#include "camadapt/CamParamsAPI.h"
#include "camadapt/ConvertJsonValidate.h"
#include "camadapt/FileIo.h"
#include "camadapt/YamlSerialization.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace libobsensor {

namespace {

bool readRawTextProperty(IDevice *owner, uint32_t propertyId, std::string &out) {
    std::vector<uint8_t> data;
    auto                 propServer = owner->getPropertyServer();

    BEGIN_TRY_EXECUTE({
        propServer->getRawData(
            static_cast<OBPropertyID>(propertyId),
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING && dataChunk && dataChunk->data && dataChunk->size > 0) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        data.clear();
        LOG_WARN("Failed to read raw property {}", static_cast<int>(propertyId));
    })

    if(data.empty()) {
        return false;
    }

    out.assign(reinterpret_cast<const char *>(data.data()), data.size());
    return true;
}

const camadapt::CameraParams *findCameraById(const camadapt::CalibrationData &calib, const std::string &cameraId) {
    auto it = std::find_if(calib.cameras.begin(), calib.cameras.end(), [&](const camadapt::CameraParams &cam) { return cam.camera_id == cameraId; });
    return it == calib.cameras.end() ? nullptr : &(*it);
}

OBCameraDistortionModel toObDistortionModel(const std::string &model) {
    std::string upper = model;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if(upper == "KB" || upper == "KANNALA_BRANDT4") {
        return OB_DISTORTION_KANNALA_BRANDT4;
    }
    if(upper == "NONE") {
        return OB_DISTORTION_NONE;
    }
    return OB_DISTORTION_BROWN_CONRADY_K6;
}

OBCameraIntrinsic toObIntrinsic(const camadapt::CameraParams &params) {
    OBCameraIntrinsic intrinsic{};
    intrinsic.fx     = static_cast<float>(params.intrinsics.fx);
    intrinsic.fy     = static_cast<float>(params.intrinsics.fy);
    intrinsic.cx     = static_cast<float>(params.intrinsics.cx);
    intrinsic.cy     = static_cast<float>(params.intrinsics.cy);
    intrinsic.width  = static_cast<int16_t>(params.image_size.width);
    intrinsic.height = static_cast<int16_t>(params.image_size.height);
    return intrinsic;
}

OBCameraDistortion toObDistortion(const camadapt::CameraParams &params) {
    OBCameraDistortion distortion{};
    distortion.k1    = static_cast<float>(params.distortion.k1);
    distortion.k2    = static_cast<float>(params.distortion.k2);
    distortion.k3    = static_cast<float>(params.distortion.k3);
    distortion.k4    = static_cast<float>(params.distortion.k4);
    distortion.k5    = static_cast<float>(params.distortion.k5);
    distortion.k6    = static_cast<float>(params.distortion.k6);
    distortion.p1    = static_cast<float>(params.distortion.p1);
    distortion.p2    = static_cast<float>(params.distortion.p2);
    distortion.model = toObDistortionModel(params.distortion.model);
    return distortion;
}

OBExtrinsic toObExtrinsic(const camadapt::Extrinsics &extr) {
    OBExtrinsic result{};
    result.rot[0]   = static_cast<float>(extr.rotation.m[0][0]);
    result.rot[1]   = static_cast<float>(extr.rotation.m[0][1]);
    result.rot[2]   = static_cast<float>(extr.rotation.m[0][2]);
    result.rot[3]   = static_cast<float>(extr.rotation.m[1][0]);
    result.rot[4]   = static_cast<float>(extr.rotation.m[1][1]);
    result.rot[5]   = static_cast<float>(extr.rotation.m[1][2]);
    result.rot[6]   = static_cast<float>(extr.rotation.m[2][0]);
    result.rot[7]   = static_cast<float>(extr.rotation.m[2][1]);
    result.rot[8]   = static_cast<float>(extr.rotation.m[2][2]);
    result.trans[0] = static_cast<float>(extr.translation.x);
    result.trans[1] = static_cast<float>(extr.translation.y);
    result.trans[2] = static_cast<float>(extr.translation.z);
    return result;
}

OBCameraIntrinsic scaleIntrinsicToProfile(const OBCameraIntrinsic &src, int targetWidth, int targetHeight) {
    if(src.width <= 0 || src.height <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        return src;
    }

    auto        out    = src;
    const float ratioX = static_cast<float>(targetWidth) / static_cast<float>(src.width);
    const float ratioY = static_cast<float>(targetHeight) / static_cast<float>(src.height);
    out.fx             = out.fx * ratioX;
    out.fy             = out.fy * ratioY;
    out.cx             = out.cx * ratioX;
    out.cy             = out.cy * ratioY;
    out.width          = static_cast<int16_t>(targetWidth);
    out.height         = static_cast<int16_t>(targetHeight);
    return out;
}

camadapt::Mat33 transpose(const camadapt::Mat33 &m) {
    camadapt::Mat33 out{};
    for(int r = 0; r < 3; ++r) {
        for(int c = 0; c < 3; ++c) {
            out.m[r][c] = m.m[c][r];
        }
    }
    return out;
}

camadapt::Mat33 mul(const camadapt::Mat33 &a, const camadapt::Mat33 &b) {
    camadapt::Mat33 out{};
    for(int r = 0; r < 3; ++r) {
        for(int c = 0; c < 3; ++c) {
            out.m[r][c] = a.m[r][0] * b.m[0][c] + a.m[r][1] * b.m[1][c] + a.m[r][2] * b.m[2][c];
        }
    }
    return out;
}

camadapt::Vec3 mul(const camadapt::Mat33 &a, const camadapt::Vec3 &v) {
    camadapt::Vec3 out{};
    out.x = a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z;
    out.y = a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z;
    out.z = a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z;
    return out;
}

camadapt::Vec3 sub(const camadapt::Vec3 &a, const camadapt::Vec3 &b) {
    camadapt::Vec3 out{};
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

bool buildExtrinsic(const camadapt::CalibrationData &calib, const std::string &fromCameraId, const std::string &toCameraId, OBExtrinsic &outExtrinsic) {
    const auto *from = findCameraById(calib, fromCameraId);
    const auto *to   = findCameraById(calib, toCameraId);
    if(!from || !to) {
        return false;
    }

    if(fromCameraId == toCameraId) {
        outExtrinsic = IdentityExtrinsics;
        return true;
    }

    auto fromRt = from->extrinsics.rotation;
    auto toRt   = to->extrinsics.rotation;
    auto fromT  = from->extrinsics.translation;
    auto toT    = to->extrinsics.translation;

    auto fromRtInv  = transpose(fromRt);
    auto outR       = mul(toRt, fromRtInv);
    auto rotatedT   = mul(outR, fromT);
    auto outT       = sub(toT, rotatedT);
    auto outCamExtr = camadapt::Extrinsics{};
    outCamExtr.rotation    = outR;
    outCamExtr.translation = outT;
    outExtrinsic           = toObExtrinsic(outCamExtr);
    return true;
}

std::string getCameraIdByIndex(const camadapt::ConvertConfig &config, size_t index) {
    if(index >= config.cameras.size()) {
        return std::string();
    }
    return config.cameras[index].id;
}

OBStreamType sensorTypeToStreamType(OBSensorType sensorType) {
    switch(sensorType) {
    case OB_SENSOR_COLOR:
        return OB_STREAM_COLOR;
    case OB_SENSOR_COLOR_LEFT:
        return OB_STREAM_COLOR_LEFT;
    case OB_SENSOR_COLOR_RIGHT:
        return OB_STREAM_COLOR_RIGHT;
    case OB_SENSOR_COLOR_SIDE_LEFT:
        return OB_STREAM_COLOR_SIDE_LEFT;
    case OB_SENSOR_COLOR_SIDE_RIGHT:
        return OB_STREAM_COLOR_SIDE_RIGHT;
    case OB_SENSOR_IR:
        return OB_STREAM_IR;
    case OB_SENSOR_IR_LEFT:
        return OB_STREAM_IR_LEFT;
    case OB_SENSOR_IR_RIGHT:
        return OB_STREAM_IR_RIGHT;
    default:
        return OB_STREAM_UNKNOWN;
    }
}

std::vector<OBStreamType> getCandidateStreamsFromDeviceSensors(IDevice *owner) {
    auto sensorTypeList = owner->getSensorTypeList();
    std::set<OBSensorType> sensorTypeSet(sensorTypeList.begin(), sensorTypeList.end());

    const std::vector<OBSensorType> preferredSensorOrder = {
        OB_SENSOR_COLOR,      OB_SENSOR_COLOR_LEFT, OB_SENSOR_COLOR_RIGHT, OB_SENSOR_COLOR_SIDE_LEFT,
        OB_SENSOR_COLOR_SIDE_RIGHT,                  OB_SENSOR_IR_LEFT,    OB_SENSOR_IR_RIGHT,
        OB_SENSOR_IR,
    };

    std::vector<OBStreamType> candidateStreams;
    for(auto sensorType: preferredSensorOrder) {
        if(sensorTypeSet.find(sensorType) == sensorTypeSet.end()) {
            continue;
        }
        auto streamType = sensorTypeToStreamType(sensorType);
        if(streamType != OB_STREAM_UNKNOWN) {
            candidateStreams.push_back(streamType);
        }
    }

    return candidateStreams;
}

bool dumpRawTextToFile(const std::string &fileName, const std::string &content) {
    auto dumpDir = utils::joinPaths(utils::getCurrentWorkDirectory(), "ego_raw_params");
    if(!utils::checkDir(dumpDir.c_str()) && utils::mkDirs(dumpDir.c_str()) != 0) {
        LOG_WARN("Failed to create dump dir: {}", dumpDir);
        return false;
    }

    auto filePath = utils::joinPaths(dumpDir, fileName);
    if(!camadapt::writeTextFile(filePath, content)) {
        LOG_WARN("Failed to dump raw text file: {}", filePath);
        return false;
    }

    LOG_INFO("Dumped raw property text to {}", filePath);
    return true;
}

bool isResolutionSupportedForCamera(const camadapt::ConvertConfig &config, const std::string &cameraId, int width, int height) {
    auto supportedCameras = camadapt::listCamerasSupportingResolution(config, width, height);
    return std::find(supportedCameras.begin(), supportedCameras.end(), cameraId) != supportedCameras.end();
}

bool validateCalibrationAgainstSensorSizes(const camadapt::ConvertConfig &config, const camadapt::CalibrationData &calib) {
    camadapt::SensorSizeConfig sensorSizes;
    for(const auto &cam : config.cameras) {
        sensorSizes.set(cam.id, cam.sensor_size.width, cam.sensor_size.height);
    }

    auto validateResult = camadapt::validateCalibrationSensor(calib, sensorSizes);
    if(!validateResult.valid) {
        LOG_ERROR("params_cam calibration sensor validation failed: code={}, msg={}", validateResult.error_code,
                  validateResult.error_message);
        return false;
    }

    return true;
}

template <size_t N>
void copyArray(double (&dst)[N], const double (&src)[N]) {
    for(size_t i = 0; i < N; ++i) {
        dst[i] = src[i];
    }
}

void applyImuIntrinsicsFromYaml(const camadapt::ImuIntrinsics &imuIntrinsics, OBIMUCalibrateParams &imuCalibParam) {
    auto &acc  = imuCalibParam.singleIMUParams[0].acc;
    auto &gyro = imuCalibParam.singleIMUParams[0].gyro;

    acc.noiseDensity  = imuIntrinsics.accel.noise_density;
    acc.randomWalk    = imuIntrinsics.accel.random_walk;
    acc.referenceTemp = imuIntrinsics.accel.reference_temp;
    copyArray(acc.bias, imuIntrinsics.accel.bias);
    copyArray(acc.gravity, imuIntrinsics.accel.gravity);
    copyArray(acc.scaleMisalignment, imuIntrinsics.accel.scale_misalignment);
    copyArray(acc.tempSlope, imuIntrinsics.accel.temp_slope);

    gyro.noiseDensity  = imuIntrinsics.gyro.noise_density;
    gyro.randomWalk    = imuIntrinsics.gyro.random_walk;
    gyro.referenceTemp = imuIntrinsics.gyro.reference_temp;
    copyArray(gyro.bias, imuIntrinsics.gyro.bias);
    copyArray(gyro.scaleMisalignment, imuIntrinsics.gyro.scale_misalignment);
    copyArray(gyro.tempSlope, imuIntrinsics.gyro.temp_slope);

    imuCalibParam.validNum = 1;
}

}  // namespace

DataAcquisitionAlgParamManager::DataAcquisitionAlgParamManager(IDevice *owner) : AlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void DataAcquisitionAlgParamManager::bindExtrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    if(!convertConfig_) {
        LOG_WARN("Skip extrinsic binding because conversion parameters are unavailable");
        return;
    }

    AlgParamManagerBase::bindExtrinsic(streamProfileList);
}

std::string DataAcquisitionAlgParamManager::getCameraIdForStreamType(OBStreamType streamType) const {
    if(!convertConfig_) {
        return std::string();
    }

    // Map stream topology to the algorithm config's camera order.
    switch(streamType) {
    case OB_STREAM_COLOR_LEFT:
        return getCameraIdByIndex(*convertConfig_, 0);
    case OB_STREAM_COLOR_RIGHT:
        return getCameraIdByIndex(*convertConfig_, 1);
    case OB_STREAM_COLOR_SIDE_RIGHT:
        return getCameraIdByIndex(*convertConfig_, 2);
    case OB_STREAM_COLOR_SIDE_LEFT:
        return getCameraIdByIndex(*convertConfig_, 3);
    case OB_STREAM_COLOR:
        return getCameraIdByIndex(*convertConfig_, 0);
    case OB_STREAM_IR_LEFT:
        return getCameraIdByIndex(*convertConfig_, 1);
    case OB_STREAM_IR_RIGHT:
        return getCameraIdByIndex(*convertConfig_, 2);
    case OB_STREAM_IR:
        return getCameraIdByIndex(*convertConfig_, 1);
    default:
        return std::string();
    }
}

void DataAcquisitionAlgParamManager::fetchParamFromDevice() {
    imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();

    std::string alignCalibYaml;
    std::string imuCalibYaml;
    std::string convertParamsJson;

    bool hasAlignCalibYaml   = readRawTextProperty(getOwner(), OB_RAW_DATA_ALIGN_CALIB_YAML, alignCalibYaml);
    bool hasImuCalibYaml     = readRawTextProperty(getOwner(), OB_RAW_DATA_IMU_CALIB_YAML, imuCalibYaml);
    bool hasConvertParamsRaw = readRawTextProperty(getOwner(), OB_RAW_DATA_CONVERT_PARAMS, convertParamsJson);
    
    if(!hasAlignCalibYaml) {
        LOG_WARN("OB_RAW_DATA_ALIGN_CALIB_YAML is unavailable");
    }
    if(!hasImuCalibYaml) {
        LOG_WARN("OB_RAW_DATA_IMU_CALIB_YAML is unavailable");
    }
    if(!hasConvertParamsRaw) {
        LOG_WARN("OB_RAW_DATA_CONVERT_PARAMS is unavailable");
    }

    if(hasAlignCalibYaml) {
        dumpRawTextToFile("align_calib.yaml", alignCalibYaml);
    }
    if(hasImuCalibYaml) {
        dumpRawTextToFile("imu_calib.yaml", imuCalibYaml);
    }
    if(hasConvertParamsRaw) {
        dumpRawTextToFile("convert_params.json", convertParamsJson);
    }

    if(hasAlignCalibYaml) {
        try {
            calibrationData_.reset(new camadapt::CalibrationData(camadapt::parseParamsYaml(alignCalibYaml)));
        }
        catch(const std::exception &e) {
            LOG_ERROR("Failed to parse OB_RAW_DATA_ALIGN_CALIB_YAML: {}", e.what());
            calibrationData_.reset();
        }
    }

    if(hasImuCalibYaml) {
        try {
            imuYamlParams_.reset(new camadapt::CameraImuParams(camadapt::parseImuYaml(imuCalibYaml)));

            auto imuIntrinsics = camadapt::parseImuIntrinsics(imuCalibYaml);
            applyImuIntrinsicsFromYaml(imuIntrinsics, imuCalibParam_);
            LOG_INFO("Applied IMU intrinsics from OB_RAW_DATA_IMU_CALIB_YAML");
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to parse OB_RAW_DATA_IMU_CALIB_YAML: {}", e.what());
            imuYamlParams_.reset();
        }
    }

    if(hasConvertParamsRaw) {
        try {
            auto validateResult = camadapt::validateConvertJson(convertParamsJson);
            if(!validateResult.valid) {
                LOG_WARN("OB_RAW_DATA_CONVERT_PARAMS validation failed: code={}, msg={}", validateResult.error_code,
                         validateResult.error_message);
            }
            convertConfig_.reset(new camadapt::ConvertConfig(camadapt::parseConvertJson(convertParamsJson)));
        }
        catch(const std::exception &e) {
            LOG_ERROR("Failed to parse OB_RAW_DATA_CONVERT_PARAMS: {}", e.what());
            convertConfig_.reset();
        }
    }

    if(convertConfig_ && calibrationData_) {
        if(!validateCalibrationAgainstSensorSizes(*convertConfig_, *calibrationData_)) {
            // Keep convert params for topology mapping, but disable invalid camera calibration data.
            calibrationData_.reset();
        }
    }

    if(imuYamlParams_) {
        auto &ext = imuYamlParams_->T_cam_imu;
        // Keep IMU intrinsics defaulted, but update IMU->camera extrinsic from params_imu.yaml.
        double imuToCam[16] = {
            ext.rotation.m[0][0], ext.rotation.m[0][1], ext.rotation.m[0][2], ext.translation.x,
            ext.rotation.m[1][0], ext.rotation.m[1][1], ext.rotation.m[1][2], ext.translation.y,
            ext.rotation.m[2][0], ext.rotation.m[2][1], ext.rotation.m[2][2], ext.translation.z,
            0.0,                  0.0,                  0.0,                  1.0,
        };
        memcpy(imuCalibParam_.singleIMUParams[0].imu_to_cam_extrinsics, imuToCam, sizeof(imuToCam));
        imuCalibParam_.singleIMUParams[0].timeshift_cam_to_imu = imuYamlParams_->timeshift_cam_imu;
    }

    calibrationIntrinsicList_.clear();
    calibrationCameraDistortionList_.clear();
    calibrationCameraParamList_.clear();
    if(convertConfig_ && calibrationData_) {
        auto candidateStreams = getCandidateStreamsFromDeviceSensors(getOwner());
        for(auto streamType: candidateStreams) {
            auto cameraId = getCameraIdForStreamType(streamType);
            if(!cameraId.empty()) {
                auto cameraParams = findCameraById(*calibrationData_, cameraId);
                if(!cameraParams) {
                    LOG_WARN("No calibration camera params for stream {}, camera_id={}", static_cast<int>(streamType), cameraId);
                    continue;
                }

                calibrationIntrinsicList_[streamType]       = toObIntrinsic(*cameraParams);
                calibrationCameraDistortionList_[streamType] = toObDistortion(*cameraParams);
            }
        }
    }
}

void DataAcquisitionAlgParamManager::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();

    for(const auto &sp: streamProfileList) {
        if(sp->is<AccelStreamProfile>()) {
            intrinsicMgr->registerAccelStreamIntrinsics(sp, imuCalibParam_.singleIMUParams[0].acc);
            continue;
        }
        if(sp->is<GyroStreamProfile>()) {
            intrinsicMgr->registerGyroStreamIntrinsics(sp, imuCalibParam_.singleIMUParams[0].gyro);
            continue;
        }

        if(!convertConfig_ || !calibrationData_) {
            continue;
        }

        auto vsp = sp->as<VideoStreamProfile>();
        if(!vsp) {
            continue;
        }

        auto cameraId = getCameraIdForStreamType(sp->getType());
        if(cameraId.empty()) {
            continue;
        }

        if(!isResolutionSupportedForCamera(*convertConfig_, cameraId, static_cast<int>(vsp->getWidth()), static_cast<int>(vsp->getHeight()))) {
            LOG_WARN("Resolution {}x{} is not supported by camera_id={} for stream {}", vsp->getWidth(), vsp->getHeight(), cameraId,
                     static_cast<int>(sp->getType()));

            auto intrIter = calibrationIntrinsicList_.find(sp->getType());
            auto distIter = calibrationCameraDistortionList_.find(sp->getType());
            if(intrIter != calibrationIntrinsicList_.end() && distIter != calibrationCameraDistortionList_.end()) {
                auto fallbackIntrinsic = scaleIntrinsicToProfile(intrIter->second, static_cast<int>(vsp->getWidth()), static_cast<int>(vsp->getHeight()));
                intrinsicMgr->registerVideoStreamIntrinsics(sp, fallbackIntrinsic);
                intrinsicMgr->registerVideoStreamDistortion(sp, distIter->second);
            }
            continue;
        }

        try {
            auto params = camadapt::getCameraParams(*convertConfig_, *calibrationData_, cameraId, static_cast<int>(vsp->getWidth()),
                                                    static_cast<int>(vsp->getHeight()), std::numeric_limits<double>::quiet_NaN());

            auto intrinsic  = toObIntrinsic(params);
            auto distortion = toObDistortion(params);
            intrinsicMgr->registerVideoStreamIntrinsics(sp, intrinsic);
            intrinsicMgr->registerVideoStreamDistortion(sp, distortion);
            calibrationIntrinsicList_[sp->getType()]        = intrinsic;
            calibrationCameraDistortionList_[sp->getType()] = distortion;
        }
        catch(const std::exception &e) {
            LOG_WARN("Failed to adapt camera params for stream {} (camera_id={}): {}", static_cast<int>(sp->getType()), cameraId, e.what());

            auto intrIter = calibrationIntrinsicList_.find(sp->getType());
            auto distIter = calibrationCameraDistortionList_.find(sp->getType());
            if(intrIter != calibrationIntrinsicList_.end() && distIter != calibrationCameraDistortionList_.end()) {
                auto fallbackIntrinsic = scaleIntrinsicToProfile(intrIter->second, static_cast<int>(vsp->getWidth()), static_cast<int>(vsp->getHeight()));
                intrinsicMgr->registerVideoStreamIntrinsics(sp, fallbackIntrinsic);
                intrinsicMgr->registerVideoStreamDistortion(sp, distIter->second);
            }
        }
    }
}

void DataAcquisitionAlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr = StreamExtrinsicsManager::getInstance();

    std::map<OBStreamType, std::string> streamToCamera;
    for(const auto &entry: calibrationIntrinsicList_) {
        auto streamType = entry.first;
        auto cameraId   = getCameraIdForStreamType(streamType);
        if(!cameraId.empty()) {
            streamToCamera[streamType] = cameraId;
        }
    }

    // Fallback: if intrinsic cache is still empty, build candidates from current sensor types.
    if(streamToCamera.empty()) {
        auto candidateStreams = getCandidateStreamsFromDeviceSensors(getOwner());
        for(auto streamType: candidateStreams) {
            auto cameraId = getCameraIdForStreamType(streamType);
            if(!cameraId.empty()) {
                streamToCamera[streamType] = cameraId;
            }
        }
    }

    std::map<OBStreamType, std::shared_ptr<const StreamProfile>> basicProfiles;
    for(const auto &entry: streamToCamera) {
        basicProfiles[entry.first] =
            StreamProfileFactory::createVideoStreamProfile(entry.first, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    }

    if(calibrationData_) {
        for(const auto &fromEntry: streamToCamera) {
            for(const auto &toEntry: streamToCamera) {
                if(fromEntry.first == toEntry.first) {
                    continue;
                }

                OBExtrinsic extrinsic{};
                if(buildExtrinsic(*calibrationData_, fromEntry.second, toEntry.second, extrinsic)) {
                    extrinsicMgr->registerExtrinsics(basicProfiles[fromEntry.first], basicProfiles[toEntry.first], extrinsic);
                }
            }
        }
    }

    OBStreamType imuReferenceStream = OB_STREAM_UNKNOWN;
    for(auto preferred: { OB_STREAM_COLOR, OB_STREAM_COLOR_LEFT, OB_STREAM_COLOR_RIGHT, OB_STREAM_COLOR_SIDE_LEFT, OB_STREAM_COLOR_SIDE_RIGHT,
                          OB_STREAM_IR_LEFT, OB_STREAM_IR_RIGHT, OB_STREAM_IR }) {
        if(streamToCamera.find(preferred) != streamToCamera.end()) {
            imuReferenceStream = preferred;
            break;
        }
    }

    auto accelBasicStreamProfile = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
    auto gyroBasicStreamProfile  = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);

    if(imuYamlParams_ && imuReferenceStream != OB_STREAM_UNKNOWN) {
        auto imuToCam = toObExtrinsic(imuYamlParams_->T_cam_imu);
        auto it       = basicProfiles.find(imuReferenceStream);
        if(it != basicProfiles.end()) {
            extrinsicMgr->registerExtrinsics(accelBasicStreamProfile, it->second, imuToCam);
        }
    }

    extrinsicMgr->registerSameExtrinsics(gyroBasicStreamProfile, accelBasicStreamProfile);

    basicStreamProfileList_.clear();
    for(const auto &entry: basicProfiles) {
        basicStreamProfileList_.push_back(entry.second);
    }
    basicStreamProfileList_.push_back(accelBasicStreamProfile);
    basicStreamProfileList_.push_back(gyroBasicStreamProfile);
}

}  // namespace libobsensor

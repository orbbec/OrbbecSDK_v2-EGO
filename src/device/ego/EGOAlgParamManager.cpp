// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGOAlgParamManager.hpp"

#include "param/AlgParseHelper.hpp"
#include "property/InternalProperty.hpp"
#include "stream/StreamIntrinsicsManager.hpp"
#include "stream/StreamExtrinsicsManager.hpp"
#include "stream/StreamProfileFactory.hpp"
#include "exception/ObException.hpp"
#include "publicfilters/IMUCorrector.hpp"

#include <vector>
#include <sstream>

namespace libobsensor {

EGOAlgParamManager::EGOAlgParamManager(IDevice *owner) : AlgParamManagerBase(owner) {
    fetchParamFromDevice();
    registerBasicExtrinsics();
}

void EGOAlgParamManager::fetchParamFromDevice() {
    // calibration camera params (left color = depthIntrinsic, right color = rgbIntrinsic, transform = left->right)
    std::vector<uint8_t> data;
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_ALIGN_CALIB_PARAM,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_ERROR("Get align calibration params failed!");
        data.clear();
    })

    if(!data.empty()) {
        auto cameraParamList = AlgParseHelper::alignCalibParamParse(data.data(), static_cast<uint32_t>(data.size()));
        for(auto &cameraParam: cameraParamList) {
            OBCameraParam param;
            param.depthIntrinsic = cameraParam.depthIntrinsic;
            param.rgbIntrinsic   = cameraParam.rgbIntrinsic;
            memcpy(&param.depthDistortion, &cameraParam.depthDistortion, sizeof(param.depthDistortion));
            param.depthDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            memcpy(&param.rgbDistortion, &cameraParam.rgbDistortion, sizeof(param.rgbDistortion));
            param.rgbDistortion.model = OB_DISTORTION_BROWN_CONRADY_K6;
            param.transform           = cameraParam.transform;
            param.isMirrored          = false;
            calibrationCameraParamList_.emplace_back(param);

            DEBUG_EXECUTE({
                std::stringstream ss;
                ss << param;
                LOG_TRACE("-{}", ss.str());
            });
        }
    }

    // EGO has two color sensors and no depth, so the D2C align profile list is intentionally not fetched.

    // imu param
    data.clear();
    BEGIN_TRY_EXECUTE({
        auto owner      = getOwner();
        auto propServer = owner->getPropertyServer();
        propServer->getRawData(
            OB_RAW_DATA_IMU_CALIB_PARAM,
            [&](OBDataTranState state, OBDataChunk *dataChunk) {
                if(state == DATA_TRAN_STAT_TRANSFERRING) {
                    data.insert(data.end(), dataChunk->data, dataChunk->data + dataChunk->size);
                }
            },
            PROP_ACCESS_INTERNAL);
    })
    CATCH_EXCEPTION_AND_EXECUTE({
        LOG_DEBUG("Get IMU calibration params failed!");  // set log level to debug as it is not an error
        data.clear();
    })

    if(!data.empty()) {
        auto realData  = data.data() + IMU_CALIBRATION_FILE_OFFSET;
        auto realSize  = static_cast<uint32_t>(data.size()) - IMU_CALIBRATION_FILE_OFFSET;
        imuCalibParam_ = IMUCorrector::parserIMUCalibParamRaw(realData, realSize);
        LOG_DEBUG("Get imu calibration params success!");
    }
    else {
        LOG_DEBUG("Get imu calibration param failed! load default param.");
        imuCalibParam_ = IMUCorrector::getDefaultImuCalibParam();
    }
}

bool EGOAlgParamManager::findBestMatchedCameraParam(const std::shared_ptr<const VideoStreamProfile> &profile, OBCameraParam &result) {
    bool found      = false;
    auto streamType = profile->getType();
    auto width      = profile->getWidth();
    auto height     = profile->getHeight();

    // left color -> depthIntrinsic, right color -> rgbIntrinsic (dual-RGB convention)
    // match same resolution
    for(const auto &param: calibrationCameraParamList_) {
        if(streamType == OB_STREAM_COLOR_LEFT && static_cast<uint32_t>(param.depthIntrinsic.width) == width
           && static_cast<uint32_t>(param.depthIntrinsic.height) == height) {
            found  = true;
            result = param;
            break;
        }
        else if(streamType == OB_STREAM_COLOR_RIGHT && static_cast<uint32_t>(param.rgbIntrinsic.width) == width
                && static_cast<uint32_t>(param.rgbIntrinsic.height) == height) {
            found  = true;
            result = param;
            break;
        }
    }

    if(!found) {
        // match same ratio
        float ratio = (float)width / height;
        for(const auto &param: calibrationCameraParamList_) {
            if(streamType == OB_STREAM_COLOR_LEFT && (float)param.depthIntrinsic.width / param.depthIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
            else if(streamType == OB_STREAM_COLOR_RIGHT && (float)param.rgbIntrinsic.width / param.rgbIntrinsic.height == ratio) {
                found  = true;
                result = param;
                break;
            }
        }
    }

    return found;
}

void EGOAlgParamManager::bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) {
    auto intrinsicMgr = StreamIntrinsicsManager::getInstance();
    for(const auto &sp: streamProfileList) {
        if(sp->is<AccelStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerAccelStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].acc);
            continue;
        }
        if(sp->is<GyroStreamProfile>()) {
            const auto &imuCalibParam = getIMUCalibrationParam();
            intrinsicMgr->registerGyroStreamIntrinsics(sp, imuCalibParam.singleIMUParams[0].gyro);
            continue;
        }

        OBCameraParam param{};
        auto          vsp = sp->as<VideoStreamProfile>();
        if(!findBestMatchedCameraParam(vsp, param)) {
            continue;
        }

        OBCameraIntrinsic  intrinsic  = { 0 };
        OBCameraDistortion distortion = { 0 };
        if(sp->getType() == OB_STREAM_COLOR_LEFT) {
            intrinsic  = param.depthIntrinsic;
            distortion = param.depthDistortion;
        }
        else {
            intrinsic  = param.rgbIntrinsic;
            distortion = param.rgbDistortion;
        }

        // Fix intrinsic from calibration to the actual profile according to the ratio of resolution
        auto ratio = (float)vsp->getWidth() / (float)intrinsic.width;
        intrinsic.fx *= ratio;
        intrinsic.fy *= ratio;
        intrinsic.cx *= ratio;
        intrinsic.cy *= ratio;
        intrinsic.width  = static_cast<int16_t>(vsp->getWidth());
        intrinsic.height = static_cast<int16_t>((float)intrinsic.height * ratio);

        intrinsicMgr->registerVideoStreamIntrinsics(sp, intrinsic);
        intrinsicMgr->registerVideoStreamDistortion(sp, distortion);
    }
}

void EGOAlgParamManager::registerBasicExtrinsics() {
    auto extrinsicMgr = StreamExtrinsicsManager::getInstance();
    auto leftColorBasicStreamProfile =
        StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR_LEFT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto rightColorBasicStreamProfile =
        StreamProfileFactory::createVideoStreamProfile(OB_STREAM_COLOR_RIGHT, OB_FORMAT_ANY, OB_WIDTH_ANY, OB_HEIGHT_ANY, OB_FPS_ANY);
    auto accelBasicStreamProfile = StreamProfileFactory::createAccelStreamProfile(OB_ACCEL_FS_2g, OB_SAMPLE_RATE_1_5625_HZ);
    auto gyroBasicStreamProfile  = StreamProfileFactory::createGyroStreamProfile(OB_GYRO_FS_16dps, OB_SAMPLE_RATE_1_5625_HZ);

    // left color -> right color extrinsic (transform holds the left-to-right transform in dual-RGB calibration)
    if(!calibrationCameraParamList_.empty()) {
        const auto &l2rExtrinsic = calibrationCameraParamList_.front().transform;
        extrinsicMgr->registerExtrinsics(leftColorBasicStreamProfile, rightColorBasicStreamProfile, l2rExtrinsic);
    }

    // imu (accel) -> left color extrinsic; the IMU calibration is referenced to the left color camera
    double imuExtr[16] = { 0 };
    memcpy(imuExtr, imuCalibParam_.singleIMUParams[0].imu_to_cam_extrinsics, sizeof(imuExtr));

    OBExtrinsic imu_to_color;
    imu_to_color.rot[0]   = (float)imuExtr[0];
    imu_to_color.rot[1]   = (float)imuExtr[1];
    imu_to_color.rot[2]   = (float)imuExtr[2];
    imu_to_color.rot[3]   = (float)imuExtr[4];
    imu_to_color.rot[4]   = (float)imuExtr[5];
    imu_to_color.rot[5]   = (float)imuExtr[6];
    imu_to_color.rot[6]   = (float)imuExtr[8];
    imu_to_color.rot[7]   = (float)imuExtr[9];
    imu_to_color.rot[8]   = (float)imuExtr[10];
    imu_to_color.trans[0] = (float)imuExtr[3];
    imu_to_color.trans[1] = (float)imuExtr[7];
    imu_to_color.trans[2] = (float)imuExtr[11];
    extrinsicMgr->registerExtrinsics(accelBasicStreamProfile, leftColorBasicStreamProfile, imu_to_color);
    extrinsicMgr->registerSameExtrinsics(gyroBasicStreamProfile, accelBasicStreamProfile);

    basicStreamProfileList_.emplace_back(leftColorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(rightColorBasicStreamProfile);
    basicStreamProfileList_.emplace_back(accelBasicStreamProfile);
    basicStreamProfileList_.emplace_back(gyroBasicStreamProfile);
}

}  // namespace libobsensor

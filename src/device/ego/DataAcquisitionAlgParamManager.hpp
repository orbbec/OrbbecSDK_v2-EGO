// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"

#include <string>

namespace camadapt {
struct ConvertConfig;
struct CalibrationData;
struct CameraImuParams;
}

namespace libobsensor {

class DataAcquisitionAlgParamManager : public AlgParamManagerBase {
public:
    DataAcquisitionAlgParamManager(IDevice *owner);
    virtual ~DataAcquisitionAlgParamManager() noexcept override = default;

    void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;

    std::string getCameraIdForStreamType(OBStreamType streamType) const;

private:
    std::shared_ptr<camadapt::ConvertConfig>   convertConfig_;
    std::shared_ptr<camadapt::CalibrationData> calibrationData_;
    std::shared_ptr<camadapt::CameraImuParams> imuYamlParams_;
    std::map<OBStreamType, OBCameraIntrinsic> calibrationIntrinsicList_;
    std::map<OBStreamType,OBCameraDistortion> calibrationCameraDistortionList_;
};

}  // namespace libobsensor

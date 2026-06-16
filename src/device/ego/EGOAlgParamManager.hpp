// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "param/AlgParamManager.hpp"
#include "stream/StreamProfile.hpp"

#include <vector>
#include <memory>

namespace libobsensor {

// Dual-RGB calibration layout
//   - depthIntrinsic / depthDistortion -> left color
//   - rgbIntrinsic   / rgbDistortion   -> right color
//   - transform                        -> left-to-right color extrinsic
class EGOAlgParamManager : public AlgParamManagerBase {
public:
    EGOAlgParamManager(IDevice *owner);
    virtual ~EGOAlgParamManager() noexcept override = default;

    void bindIntrinsic(std::vector<std::shared_ptr<const StreamProfile>> streamProfileList) override;

private:
    void fetchParamFromDevice() override;
    void registerBasicExtrinsics() override;

    bool findBestMatchedCameraParam(const std::shared_ptr<const VideoStreamProfile> &profile, OBCameraParam &result);
};

}  // namespace libobsensor

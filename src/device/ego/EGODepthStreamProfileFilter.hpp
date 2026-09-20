// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IStreamProfile.hpp"
#include "InternalTypes.hpp"
#include "stream/StreamProfile.hpp"

#include <utility>
#include <vector>

namespace libobsensor {

// Keeps only the stream profiles that are actually usable in the device's current depth work mode.
// Shared by the USB variant (EGODepthDevice) and the network variant (EGODepthNetDevice): both read the
// effective profile list from OB_RAW_DATA_EFFECTIVE_VIDEO_STREAM_PROFILE_LIST and attach one instance
// per sensor whose profile set depends on the work mode.
class EGODepthEffectiveProfileFilter : public IStreamProfileFilter {
public:
    EGODepthEffectiveProfileFilter(OBSensorType sensorType, std::vector<OBEffectiveStreamProfile> effectiveProfiles)
        : sensorType_(sensorType), effectiveProfiles_(std::move(effectiveProfiles)) {}

    StreamProfileList filter(const StreamProfileList &profiles) const override {
        StreamProfileList filteredProfiles;
        for(const auto &profile: profiles) {
            if(!profile->is<VideoStreamProfile>()) {
                filteredProfiles.push_back(profile);
                continue;
            }

            auto videoProfile = profile->as<VideoStreamProfile>();
            auto format       = videoProfile->getFormat() == OB_FORMAT_HEVC ? OB_FORMAT_H265 : videoProfile->getFormat();
            for(const auto &effectiveProfile: effectiveProfiles_) {
                auto effectiveFormat = effectiveProfile.format == OB_FORMAT_HEVC ? OB_FORMAT_H265 : effectiveProfile.format;
                if(effectiveProfile.sensorType == sensorType_ && effectiveFormat == format && effectiveProfile.width == videoProfile->getWidth()
                   && effectiveProfile.height == videoProfile->getHeight() && videoProfile->getFps() <= effectiveProfile.maxFps) {
                    filteredProfiles.push_back(profile);
                    break;
                }
            }
        }
        return filteredProfiles;
    }

private:
    OBSensorType                          sensorType_;
    std::vector<OBEffectiveStreamProfile> effectiveProfiles_;
};

}  // namespace libobsensor

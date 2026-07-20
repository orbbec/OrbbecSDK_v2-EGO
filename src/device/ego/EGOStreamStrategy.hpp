// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "DeviceComponentBase.hpp"
#include "ISensorStreamStrategy.hpp"
#include "InternalTypes.hpp"

#include <vector>

namespace libobsensor {

class EGOStreamStrategy : public ISensorStreamStrategy, public DeviceComponentBase {
public:
    explicit EGOStreamStrategy(IDevice *owner);
    ~EGOStreamStrategy() noexcept override = default;

    void validateStream(const std::shared_ptr<const StreamProfile> &profile) override;
    void validateStream(const std::vector<std::shared_ptr<const StreamProfile>> &profiles) override;
    void markStreamActivated(const std::shared_ptr<const StreamProfile> &profile) override;
    void markStreamDeactivated(const std::shared_ptr<const StreamProfile> &profile) override;

private:
    bool                                         isRecording() const;
    std::vector<OBInternalStreamProfile>         fetchRecordingProfiles() const;
    void validateRecordingProfile(const std::shared_ptr<const StreamProfile> &profile,
                                  const std::vector<OBInternalStreamProfile> &recordingProfiles) const;
};

}  // namespace libobsensor

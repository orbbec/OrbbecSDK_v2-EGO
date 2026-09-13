// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once
#include "IDevice.hpp"
#include "IPresetManager.hpp"
#include "DeviceComponentBase.hpp"

#include <string>
#include <vector>

namespace libobsensor {

class EGODepthPresetManager : public IPresetManager, public DeviceComponentBase {
public:
    EGODepthPresetManager(IDevice *owner);
    virtual ~EGODepthPresetManager() noexcept override = default;

    void                            loadPreset(const std::string &presetName) override;
    const std::string              &getCurrentPresetName() const override;
    const std::vector<std::string> &getAvailablePresetList() const override;
    void                            loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) override;
    void                            loadPresetFromJsonFile(const std::string &filePath) override;
    const std::vector<uint8_t>     &exportSettingsAsPresetJsonData(const std::string &presetName) override;
    void                            exportSettingsAsPresetJsonFile(const std::string &filePath) override;
    void                            fetchPreset() override;

private:
    std::vector<std::string> availablePresets_;
    std::string              currentPreset_;
};

}  // namespace libobsensor

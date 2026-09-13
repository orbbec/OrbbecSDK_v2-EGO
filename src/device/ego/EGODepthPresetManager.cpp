// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODepthPresetManager.hpp"
#include "IDepthWorkModeManager.hpp"
#include "exception/ObException.hpp"

#include <algorithm>

namespace libobsensor {

EGODepthPresetManager::EGODepthPresetManager(IDevice *owner) : DeviceComponentBase(owner) {
    auto depthWorkModeManager = owner->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    for(auto &mode: depthWorkModeManager->getDepthWorkModeList()) {
        availablePresets_.emplace_back(mode.name);
    }
    currentPreset_ = depthWorkModeManager->getCurrentDepthWorkMode().name;
}

void EGODepthPresetManager::loadPreset(const std::string &presetName) {
    if(std::find(availablePresets_.begin(), availablePresets_.end(), presetName) == availablePresets_.end()) {
        THROW_INVALID_PARAM_EXCEPTION("Invalid preset name: " + presetName);
    }

    auto depthWorkModeManager = getOwner()->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);
    depthWorkModeManager->switchDepthWorkMode(presetName.c_str());
    currentPreset_ = presetName;
}

const std::string &EGODepthPresetManager::getCurrentPresetName() const {
    return currentPreset_;
}

const std::vector<std::string> &EGODepthPresetManager::getAvailablePresetList() const {
    return availablePresets_;
}

void EGODepthPresetManager::loadPresetFromJsonData(const std::string &presetName, const std::vector<uint8_t> &jsonData) {
    (void)presetName;
    (void)jsonData;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("EGODepth preset does not support JSON import");
}

void EGODepthPresetManager::loadPresetFromJsonFile(const std::string &filePath) {
    (void)filePath;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("EGODepth preset does not support JSON import");
}

const std::vector<uint8_t> &EGODepthPresetManager::exportSettingsAsPresetJsonData(const std::string &presetName) {
    (void)presetName;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("EGODepth preset does not support JSON export");
}

void EGODepthPresetManager::exportSettingsAsPresetJsonFile(const std::string &filePath) {
    (void)filePath;
    THROW_UNSUPPORTED_OPERATION_EXCEPTION("EGODepth preset does not support JSON export");
}

void EGODepthPresetManager::fetchPreset() {
    auto depthWorkModeManager = getOwner()->getComponentT<IDepthWorkModeManager>(OB_DEV_COMPONENT_DEPTH_WORK_MODE_MANAGER);

    // refetch list from firmware
    depthWorkModeManager->fetchDepthWorkModeList();

    availablePresets_.clear();
    for(auto &mode: depthWorkModeManager->getDepthWorkModeList()) {
        availablePresets_.emplace_back(mode.name);
    }
    currentPreset_ = depthWorkModeManager->getCurrentDepthWorkMode().name;
}

}  // namespace libobsensor

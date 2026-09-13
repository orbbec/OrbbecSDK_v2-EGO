// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#include "EGODepthDepthWorkModeManager.hpp"
#include "component/property/InternalProperty.hpp"
#include "exception/ObException.hpp"
#include "logger/Logger.hpp"
#include "utils/Utils.hpp"

#include <algorithm>
#include <cstring>
#include <string>

namespace libobsensor {

EGODepthDepthWorkModeManager::EGODepthDepthWorkModeManager(IDevice *owner) : DeviceComponentBase(owner) {
    fetchDepthWorkModeList();
}

std::vector<OBDepthWorkMode_Internal> EGODepthDepthWorkModeManager::getDepthWorkModeList() const {
    return depthWorkModeList_;
}

const OBDepthWorkMode_Internal &EGODepthDepthWorkModeManager::getCurrentDepthWorkMode() const {
    return currentWorkMode_;
}

void EGODepthDepthWorkModeManager::fetchDepthWorkModeList() {
    auto owner      = getOwner();
    auto propServer = owner->getPropertyServer();

    // EGODepth firmware may not implement the depth-alg-mode structures yet. Keep the list empty on
    // failure: EGODepthDevice::fixSensorList() treats an empty list as "keep the legacy full sensor set".
    try {
        depthWorkModeList_ = propServer->getStructureDataListProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_RAW_DATA_DEPTH_ALG_MODE_LIST);
        currentWorkMode_   = propServer->getStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE);
    }
    catch(const std::exception &e) {
        LOG_WARN("EGODepth depth work mode not supported by firmware: {}", e.what());
        depthWorkModeList_.clear();
    }
}

void EGODepthDepthWorkModeManager::switchDepthWorkMode(const std::string &modeName) {
    auto iter = std::find_if(depthWorkModeList_.begin(), depthWorkModeList_.end(),
                             [&modeName](const OBDepthWorkMode_Internal &mode) { return modeName == mode.name; });

    if(iter == depthWorkModeList_.end()) {
        std::string totalNames;
        std::for_each(depthWorkModeList_.begin(), depthWorkModeList_.end(), [&totalNames](const OBDepthWorkMode_Internal &mode) {
            if(!totalNames.empty()) {
                totalNames += ",";
            }
            totalNames += mode.name;
        });
        THROW_UNSUPPORTED_OPERATION_EXCEPTION("Invalid depth mode: " + modeName + ", support depth work mode list: " + totalNames);
    }

    switchDepthWorkMode(*iter);
}

void EGODepthDepthWorkModeManager::switchDepthWorkMode(const OBDepthWorkMode_Internal &targetDepthMode) {
    auto owner = getOwner();
    // Get the property server first to lock the resource, so no stream can be started concurrently.
    auto propServer = owner->getPropertyServer();

    if(owner->hasAnySensorStreamActivated()) {
        THROW_UNSUPPORTED_OPERATION_EXCEPTION(utils::string::to_string()
                                              << "Cannot switch depth work mode while any stream is started. Please stop all stream first!");
    }

    if(std::strncmp(currentWorkMode_.name, targetDepthMode.name, sizeof(targetDepthMode.name)) == 0) {
        LOG_DEBUG("switchDepthWorkMode done with same mode: {}", currentWorkMode_.name);
        return;
    }

    propServer->setStructureDataProtoV1_1_T<OBDepthWorkMode_Internal, 0>(OB_STRUCT_CURRENT_DEPTH_ALG_MODE, targetDepthMode);
    currentWorkMode_ = targetDepthMode;

    LOG_DEBUG("EGODepth depth work mode switched to: {}, device will be reinitialized to apply the new mode.", targetDepthMode.name);

    // EGODepth has exactly two modes (Default / Factory Calib) whose sensor sets differ, so every
    // cross-mode switch must rebuild the device tree (same as 305's Dual Color boundary switch).
    owner->reset();
}

}  // namespace libobsensor

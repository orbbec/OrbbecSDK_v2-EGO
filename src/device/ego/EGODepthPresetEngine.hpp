// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "preset/PresetEngine.hpp"
#include "utils/jsonmodel/Handler.hpp"

namespace libobsensor {

// Current preset version
constexpr const uint32_t kG330PresetVersion = 3;

/**
 * @brief EGODepthPresetEngine version 1
 */
class EGODepthPresetEngineV1 : public PresetEngineBase {
public:
    EGODepthPresetEngineV1(IDevice *owner);
    ~EGODepthPresetEngineV1() override = default;

    /**
     * @brief Implementation of IPresetEngine::init
     */
    void init() override;
};

/**
 * @brief EGODepthPresetEngine (api_version.preset >= 2)
 */
class EGODepthPresetEngine : public PresetEngineBase {
public:
    EGODepthPresetEngine(IDevice *owner, uint32_t presetVersion);
    ~EGODepthPresetEngine() override = default;

    /**
     * @brief Implementation of IPresetEngine::init
     */
    void init() override;

private:
    uint32_t presetVersion_{ kG330PresetVersion };
};

}  // namespace libobsensor

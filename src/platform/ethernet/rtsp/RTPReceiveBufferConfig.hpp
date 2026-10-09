// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include <cstdint>

namespace libobsensor {

enum class RTPReceiveMode : uint8_t {
    Legacy,
    ProfileDropOldest,
};

struct RTPReceiveBufferConfig {
    RTPReceiveMode mode                     = RTPReceiveMode::ProfileDropOldest;
    uint32_t       preferredBufferCount     = 16;
    uint32_t       minBufferCount           = 4;
    uint32_t       maxPayloadBytes          = 0;
    uint64_t       perSinkMemoryBudgetBytes = 256ULL * 1024ULL * 1024ULL;
    uint32_t       maxReadyQueueDelayMs     = 80;
    bool           waitForKeyFrameAfterDrop = true;
};

}  // namespace libobsensor

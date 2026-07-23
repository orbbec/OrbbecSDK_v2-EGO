// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace libobsensor {
class EGOMd5 {
public:
    EGOMd5();
    void                    update(const uint8_t *data, size_t size);
    std::array<uint8_t, 16> finish();

private:
    void     transform(const uint8_t block[64]);
    uint32_t state_[4];
    uint64_t bitCount_;
    uint8_t  buffer_[64];
    size_t   buffered_;
    bool     finished_;
};
std::array<uint8_t, 16> egoMd5(const uint8_t *data, size_t size);
}  // namespace libobsensor

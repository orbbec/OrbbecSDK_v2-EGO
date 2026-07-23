// Copyright (c) Orbbec Inc. All Rights Reserved.
// Licensed under the MIT License.
#include "EGOMd5.hpp"
#include <cstring>
namespace libobsensor {
namespace {
uint32_t rot(uint32_t x, uint32_t n) {
    return (x << n) | (x >> (32 - n));
}
uint32_t load32(const uint8_t *p) {
    return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
void store32(uint8_t *p, uint32_t x) {
    p[0] = uint8_t(x);
    p[1] = uint8_t(x >> 8);
    p[2] = uint8_t(x >> 16);
    p[3] = uint8_t(x >> 24);
}
const uint32_t K[64] = { 0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1,
                         0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453,
                         0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942,
                         0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
                         0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d,
                         0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };
const uint8_t  S[64] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
                         4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };
}  // namespace
EGOMd5::EGOMd5() : state_{ 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 }, bitCount_(0), buffer_{}, buffered_(0), finished_(false) {}
void EGOMd5::transform(const uint8_t block[64]) {
    uint32_t m[16];
    for(int i = 0; i < 16; ++i) {
        m[i] = load32(block + 4 * i);
    }
    uint32_t a = state_[0], b = state_[1], c = state_[2], d = state_[3];
    for(uint32_t i = 0; i < 64; ++i) {
        uint32_t f, g;
        if(i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        }
        else if(i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        }
        else if(i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        }
        else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        uint32_t t = d;
        d          = c;
        c          = b;
        b          = b + rot(a + f + K[i] + m[g], S[i]);
        a          = t;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
}
void EGOMd5::update(const uint8_t *data, size_t size) {
    if(finished_ || (!data && size)) {
        return;
    }
    bitCount_ += uint64_t(size) * 8;
    while(size) {
        size_t n = 64 - buffered_;
        if(n > size) {
            n = size;
        }
        std::memcpy(buffer_ + buffered_, data, n);
        buffered_ += n;
        data += n;
        size -= n;
        if(buffered_ == 64) {
            transform(buffer_);
            buffered_ = 0;
        }
    }
}
std::array<uint8_t, 16> EGOMd5::finish() {
    uint64_t bits    = bitCount_;
    uint8_t  pad[64] = { 0x80 };
    size_t n = 120 - buffered_;
    if(buffered_ < 56) {
        n = 56 - buffered_;
    }
    update(pad, n);
    uint8_t len[8];
    for(int i = 0; i < 8; ++i) {
        len[i] = uint8_t(bits >> (8 * i));
    }
    update(len, 8);
    std::array<uint8_t, 16> out{};
    for(int i = 0; i < 4; ++i) {
        store32(out.data() + 4 * i, state_[i]);
    }
    finished_ = true;
    return out;
}
std::array<uint8_t, 16> egoMd5(const uint8_t *d, size_t n) {
    EGOMd5 m;
    m.update(d, n);
    return m.finish();
}
}  // namespace libobsensor

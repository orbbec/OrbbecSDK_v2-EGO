#pragma once
#include <cstdint>

/**
 * 鏂囦欢鏉ユ簮锛氫竵婊★紝2022骞?鏈?鏃? * 鍘熷鏂囦欢鍛藉悕锛歝rc.h
 * 鏇存敼锛?. 澧炲姞C++鍛藉悕绌洪棿
 */

namespace alg_write_flash {

#define USE_CRC32_CHECK_FUN
// #define USE_CRC16_CHECK_FUN

#ifdef USE_CRC16_CHECK_FUN

uint16_t CRC16(uint8_t *ptr, unsigned long length);

#endif

#ifdef USE_CRC32_CHECK_FUN

uint32_t CRC32(uint32_t crc_in, uint8_t *buf, uint32_t buf_len);
int      crc32Check(uint8_t *buffer, uint32_t length, uint32_t receivedCrc);

#endif

}  // namespace alg_write_flash

#ifndef SERCOM_BYTE_CODEC_H
#define SERCOM_BYTE_CODEC_H

#include <cstdint>

namespace sercom::ByteCodec {

/*
 * Store a 16 bit value into two bytes with the least significant byte first.
 */
void storeUint16(uint8_t *destination, uint16_t value);

/*
 * Store a 32 bit value into four bytes with the least significant byte first.
 */
void storeUint32(uint8_t *destination, uint32_t value);

/*
 * Load a 16 bit value from two bytes with the least significant byte first.
 */
[[nodiscard]] uint16_t loadUint16(const uint8_t *source);

/*
 * Load a 32 bit value from four bytes with the least significant byte first.
 */
[[nodiscard]] uint32_t loadUint32(const uint8_t *source);

} // namespace sercom::ByteCodec

#endif // SERCOM_BYTE_CODEC_H

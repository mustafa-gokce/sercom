#include "../include/ByteCodec.h"

namespace sercom::ByteCodec {

void storeUint16(uint8_t *destination, const uint16_t value)
{
    // store the bytes from the least significant one
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
}

void storeUint32(uint8_t *destination, const uint32_t value)
{
    // store the bytes from the least significant one
    destination[0] = static_cast<uint8_t>(value);
    destination[1] = static_cast<uint8_t>(value >> 8);
    destination[2] = static_cast<uint8_t>(value >> 16);
    destination[3] = static_cast<uint8_t>(value >> 24);
}

uint16_t loadUint16(const uint8_t *source)
{
    // join the bytes from the least significant one
    return static_cast<uint16_t>(source[0] | (source[1] << 8));
}

uint32_t loadUint32(const uint8_t *source)
{
    // join the bytes from the least significant one
    return static_cast<uint32_t>(source[0]) | (static_cast<uint32_t>(source[1]) << 8)
           | (static_cast<uint32_t>(source[2]) << 16) | (static_cast<uint32_t>(source[3]) << 24);
}

} // namespace sercom::ByteCodec

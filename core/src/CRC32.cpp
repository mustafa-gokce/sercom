#include "../include/CRC32.h"

#include <array>
#include <numeric>

#include "../include/Definitions.h"

namespace sercom::CRC32 {

uint32_t calculateCRC32(const uint8_t *data, const size_t size)
{
    // expose the register after the final XOR
    return updateCRC32(Definitions::CRC32_INITIAL_VALUE, data, size)
           ^ Definitions::CRC32_FINAL_XOR_VALUE;
}

uint32_t updateCRC32(const uint32_t crc, const uint8_t *data, const size_t size)
{
    // data must exist
    if (data == nullptr) {
        return crc;
    }

    // build the lookup table once, one entry per byte value
    static const std::array<uint32_t, 256> table = []() {
        // run the bitwise algorithm for every byte value
        std::array<uint32_t, 256> lookupTable = std::array<uint32_t, 256>();
        for (size_t index = 0; index < lookupTable.size(); ++index) {
            uint32_t value = static_cast<uint32_t>(index);
            for (int bit = 0; bit < 8; ++bit) {
                const uint32_t polynomial = (value & 1) != 0 ? Definitions::CRC32_POLYNOMIAL : 0;
                value = (value >> 1) ^ polynomial;
            }
            lookupTable[index] = value;
        }

        // expose the lookup table
        return lookupTable;
    }();

    // expose the register after one table lookup per byte
    return std::accumulate(data, data + size, crc, [](const uint32_t value, const uint8_t byte) {
        return (value >> 8) ^ table[(value ^ byte) & 0xFF];
    });
}

} // namespace sercom::CRC32

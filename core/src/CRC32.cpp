#include "../include/CRC32.h"

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

    // shift the byte through the register from its lowest bit
    const auto updateRegister = [](const uint32_t value, const uint8_t byte) {
        // fold the byte into the register
        uint32_t shiftRegister = value ^ byte;
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t polynomial = (shiftRegister & 1) != 0 ? Definitions::CRC32_POLYNOMIAL
                                                                 : 0;
            shiftRegister = (shiftRegister >> 1) ^ polynomial;
        }

        // expose the register
        return shiftRegister;
    };

    // expose the register after eight shift steps per byte
    return std::accumulate(data, data + size, crc, updateRegister);
}

} // namespace sercom::CRC32

#ifndef SERCOM_CRC32_H
#define SERCOM_CRC32_H

#include <cstddef>
#include <cstdint>

namespace sercom::CRC32 {

/*
 * Calculate the CRC32 of the data.
 */
[[nodiscard]] uint32_t calculateCRC32(const uint8_t *data, size_t size);

/*
 * Fold the data into a running CRC32 register.
 * The register starts at the initial value and turns into the CRC32 with the final XOR.
 */
[[nodiscard]] uint32_t updateCRC32(uint32_t crc, const uint8_t *data, size_t size);

} // namespace sercom::CRC32

#endif // SERCOM_CRC32_H

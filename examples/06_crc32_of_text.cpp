#include <bitset>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"
#include "../host/include/HostUtils.h"

/*
 * Get a CRC32 as hex text with all of its digits.
 */
[[nodiscard]] std::string getCRC32Text(uint32_t crc);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;

    // calculate the CRC32 of the check text
    static constexpr std::string_view checkText = "123456789";
    static constexpr uint32_t checkValue = 0xCBF43926U;
    const std::vector<uint8_t> checkTextAsBytes(checkText.cbegin(), checkText.cend());
    const uint32_t crc = CRC32::calculateCRC32(checkTextAsBytes.data(), checkTextAsBytes.size());

    // print the CRC32 next to the check value
    const std::string line = HostUtils::toEscapedString(checkTextAsBytes.data(),
                                                        checkTextAsBytes.size())
                             + " -> CRC32 " + getCRC32Text(crc) + ", check value "
                             + getCRC32Text(checkValue);
    HostUtils::printLine(line);

    // CRC32 must be the check value
    if (crc != checkValue) {
        HostUtils::logWarning("CRC32 of the check text is not the check value");
        return 1;
    }

    // flip each bit of the last character in a copy of the text
    static constexpr size_t crcBitCount = 8 * Definitions::CRC_SIZE;
    size_t undetectedFlipCount = 0;
    HostUtils::printLine("flip one bit of the last character");
    for (int bit = 0; bit < 8; ++bit) {
        // flip the bit
        std::vector<uint8_t> flippedBytes = checkTextAsBytes;
        flippedBytes.back() = static_cast<uint8_t>(flippedBytes.back() ^ (1 << bit));

        // calculate the CRC32 and count its changed bits
        const uint32_t flippedCRC = CRC32::calculateCRC32(flippedBytes.data(), flippedBytes.size());
        const size_t changedBitCount = std::bitset<crcBitCount>(crc ^ flippedCRC).count();

        // print the changed text with its CRC32
        const std::string flipLine = "  bit " + std::to_string(bit) + " "
                                     + HostUtils::toEscapedString(flippedBytes.data(),
                                                                  flippedBytes.size())
                                     + " -> CRC32 " + getCRC32Text(flippedCRC) + ", "
                                     + std::to_string(changedBitCount) + " of "
                                     + std::to_string(crcBitCount) + " bits changed";
        HostUtils::printLine(flipLine);

        // count a flip that the CRC32 misses
        if (flippedCRC == crc) {
            ++undetectedFlipCount;
        }
    }

    // CRC32 must detect every flipped bit
    if (undetectedFlipCount != 0) {
        HostUtils::logWarning("CRC32 missed a flipped bit");
        return 1;
    }

    // example finished successfully
    return 0;
}

std::string getCRC32Text(const uint32_t crc)
{
    using namespace sercom;

    // expose the hex digits of the CRC32 field
    static constexpr size_t digitCount = 2 * Definitions::CRC_SIZE;
    return "0x" + HostUtils::getHexText(crc, digitCount);
}

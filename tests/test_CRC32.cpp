#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/CRC32.h"
#include "../core/include/Definitions.h"

using namespace sercom;

class TestCRC32
{
public:
    /*
     * CRC32 test fixture.
     */
    TestCRC32()
    {
        // initialize random bytes with a fixed seed
        static constexpr uint32_t randomSeed = 1234;
        std::mt19937 generator(randomSeed);
        std::uniform_int_distribution<int> byteDistribution(0, std::numeric_limits<uint8_t>::max());
        this->randomBytes.resize(this->RandomByteCount);
        std::generate(this->randomBytes.begin(),
                      this->randomBytes.end(),
                      [&generator, &byteDistribution]() {
                          return static_cast<uint8_t>(byteDistribution(generator));
                      });
    }
    ~TestCRC32() = default;

    /*
     * Calculate the CRC32 bit by bit as the reference of the table driven CRC32.
     */
    [[nodiscard]] static uint32_t calculateBitwiseCRC32(const uint8_t *data, const size_t size)
    {
        // data must exist
        if (data == nullptr) {
            return Definitions::CRC32_INITIAL_VALUE ^ Definitions::CRC32_FINAL_XOR_VALUE;
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

        // expose the register of every byte after the final XOR
        return std::accumulate(data, data + size, Definitions::CRC32_INITIAL_VALUE, updateRegister)
               ^ Definitions::CRC32_FINAL_XOR_VALUE;
    }

    /*
     * Get the bytes of a text.
     */
    [[nodiscard]] static std::vector<uint8_t> getBytes(const std::string &text)
    {
        // expose the characters as bytes
        return std::vector<uint8_t>(text.cbegin(), text.cend());
    }

    // delete copy and move operators
    TestCRC32(const TestCRC32 &other) = delete;
    TestCRC32 &operator=(const TestCRC32 &other) = delete;
    TestCRC32(TestCRC32 &&other) noexcept = delete;
    TestCRC32 &operator=(TestCRC32 &&other) noexcept = delete;

protected:
    static constexpr size_t RandomByteCount = 1024; // number of random bytes

protected:
    std::vector<uint8_t> randomBytes; // random bytes of a fixed seed
};

TEST_CASE_METHOD(TestCRC32,
                 "CRC32 of the standard check string matches the published value",
                 "[CRC32][testCheckValue]")
{
    // get the bytes of the standard check string
    const std::vector<uint8_t> checkBytes = getBytes("123456789");

    // check the published check value
    REQUIRE(CRC32::calculateCRC32(checkBytes.data(), checkBytes.size()) == 0xCBF43926U);
    REQUIRE(calculateBitwiseCRC32(checkBytes.data(), checkBytes.size()) == 0xCBF43926U);
}

TEST_CASE_METHOD(TestCRC32, "CRC32 of no data is zero", "[CRC32][testEmptyData]")
{
    // check missing and empty data
    REQUIRE(CRC32::calculateCRC32(nullptr, 0) == 0x00000000U);
    REQUIRE(CRC32::calculateCRC32(randomBytes.data(), 0) == 0x00000000U);
    REQUIRE(calculateBitwiseCRC32(nullptr, 0) == 0x00000000U);
}

TEST_CASE_METHOD(TestCRC32,
                 "CRC32 matches the known values of zlib and Python",
                 "[CRC32][testKnownValues]")
{
    // check a single zero byte
    const std::vector<uint8_t> zeroBytes = {0x00};
    REQUIRE(CRC32::calculateCRC32(zeroBytes.data(), zeroBytes.size()) == 0xD202EF8DU);

    // check the pangram of the quick brown fox
    const std::vector<uint8_t> foxBytes = getBytes("The quick brown fox jumps over the lazy dog");
    REQUIRE(CRC32::calculateCRC32(foxBytes.data(), foxBytes.size()) == 0x414FA339U);
}

TEST_CASE_METHOD(TestCRC32,
                 "Table driven and bitwise CRC32 agree on random data",
                 "[CRC32][testBitwiseReference]")
{
    // compare both CRC32 on growing parts of the random bytes
    static constexpr size_t sizeStep = 37;
    for (size_t size = 0; size <= randomBytes.size(); size += sizeStep) {
        CAPTURE(size);
        REQUIRE(CRC32::calculateCRC32(randomBytes.data(), size)
                == calculateBitwiseCRC32(randomBytes.data(), size));
    }
}

TEST_CASE_METHOD(TestCRC32,
                 "Incremental updates give the same CRC32 as one call",
                 "[CRC32][testIncrementalUpdate]")
{
    // calculate the CRC32 of the random bytes in one call
    const uint32_t crc = CRC32::calculateCRC32(randomBytes.data(), randomBytes.size());

    // iterate over the split points of the random bytes
    static const std::vector<size_t> splitPoints = {0, 1, 100, 513, RandomByteCount};
    std::for_each(splitPoints.cbegin(), splitPoints.cend(), [this, crc](const size_t splitPoint) {
        // update the CRC32 in two parts
        CAPTURE(splitPoint);
        const uint32_t firstPartCRC = CRC32::updateCRC32(Definitions::CRC32_INITIAL_VALUE,
                                                         randomBytes.data(),
                                                         splitPoint);
        const uint32_t secondPartCRC = CRC32::updateCRC32(firstPartCRC,
                                                          randomBytes.data() + splitPoint,
                                                          randomBytes.size() - splitPoint);
        REQUIRE((secondPartCRC ^ Definitions::CRC32_FINAL_XOR_VALUE) == crc);
    });
}

TEST_CASE_METHOD(TestCRC32, "Every single bit flip changes the CRC32", "[CRC32][testSingleBitFlip]")
{
    // calculate the CRC32 of the first random bytes
    static constexpr size_t dataSize = 64;
    std::vector<uint8_t> data(randomBytes.cbegin(), randomBytes.cbegin() + dataSize);
    const uint32_t crc = CRC32::calculateCRC32(data.data(), data.size());

    // iterate over every bit of the data
    for (size_t byteIndex = 0; byteIndex < data.size(); ++byteIndex) {
        CAPTURE(byteIndex);
        for (int bit = 0; bit < 8; ++bit) {
            // flip the bit and flip it back
            CAPTURE(bit);
            data.at(byteIndex) = static_cast<uint8_t>(data.at(byteIndex) ^ (1 << bit));
            REQUIRE_FALSE(CRC32::calculateCRC32(data.data(), data.size()) == crc);
            data.at(byteIndex) = static_cast<uint8_t>(data.at(byteIndex) ^ (1 << bit));
        }
    }
}

TEST_CASE_METHOD(TestCRC32,
                 "Every burst of up to 32 bits changes the CRC32",
                 "[CRC32][testBurstError]")
{
    // calculate the CRC32 of the first random bytes
    static constexpr size_t dataSize = 32;
    const std::vector<uint8_t> data(randomBytes.cbegin(), randomBytes.cbegin() + dataSize);
    const uint32_t crc = CRC32::calculateCRC32(data.data(), data.size());

    // iterate over the bursts at every seventh bit
    static constexpr size_t burstLength = 32;
    static constexpr size_t startBitStep = 7;
    for (size_t startBit = 0; startBit + burstLength <= data.size() * 8; startBit += startBitStep) {
        // flip the first and the last bit of the burst
        CAPTURE(startBit);
        std::vector<uint8_t> damagedData = data;
        const size_t endBit = startBit + burstLength - 1;
        damagedData.at(startBit / 8) = static_cast<uint8_t>(damagedData.at(startBit / 8)
                                                            ^ (1 << (startBit % 8)));
        damagedData.at(endBit / 8) = static_cast<uint8_t>(damagedData.at(endBit / 8)
                                                          ^ (1 << (endBit % 8)));
        REQUIRE_FALSE(CRC32::calculateCRC32(damagedData.data(), damagedData.size()) == crc);
    }
}

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../firmware/common/include/CommonSettings.h"
#include "../firmware/common/include/RingBuffer.h"

using namespace sercom;

class TestRingBuffer
{
public:
    /*
     * Ring buffer test fixture.
     */
    TestRingBuffer()
    {
        // initialize ring buffer
        this->ringBuffer.reset(new RingBuffer::RingBuffer());
    }
    ~TestRingBuffer()
    {
        // close ring buffer
        this->ringBuffer.reset(nullptr);
    }

    /*
     * Get bytes that count up from a first value.
     */
    [[nodiscard]] static std::vector<uint8_t> getCountingBytes(const size_t size,
                                                               const uint8_t firstValue)
    {
        // count up from the first value
        std::vector<uint8_t> bytes(size, 0);
        std::iota(bytes.begin(), bytes.end(), firstValue);

        // expose the bytes
        return bytes;
    }

    // delete copy and move operators
    TestRingBuffer(const TestRingBuffer &other) = delete;
    TestRingBuffer &operator=(const TestRingBuffer &other) = delete;
    TestRingBuffer(TestRingBuffer &&other) noexcept = delete;
    TestRingBuffer &operator=(TestRingBuffer &&other) noexcept = delete;

protected:
    std::unique_ptr<RingBuffer::RingBuffer> ringBuffer; // ring buffer under test
};

TEST_CASE_METHOD(TestRingBuffer,
                 "Bytes leave the ring in the order they were written",
                 "[RingBuffer][testOrder]")
{
    // write three bytes one by one
    static constexpr size_t byteCount = 3;
    static constexpr uint8_t firstValue = 0x41;
    const std::vector<uint8_t> bytes = getCountingBytes(byteCount, firstValue);
    REQUIRE(ringBuffer->getEmpty());
    REQUIRE(ringBuffer->writeByte(bytes.at(0)));
    REQUIRE(ringBuffer->writeByte(bytes.at(1)));
    REQUIRE(ringBuffer->writeByte(bytes.at(2)));
    REQUIRE_FALSE(ringBuffer->getEmpty());
    REQUIRE(ringBuffer->getAvailableSize() == CommonSettings::RING_BUFFER_SIZE - 1 - byteCount);

    // read them back one by one
    uint8_t byte = 0;
    REQUIRE(ringBuffer->readByte(byte));
    REQUIRE(byte == bytes.at(0));
    REQUIRE(ringBuffer->readByte(byte));
    REQUIRE(byte == bytes.at(1));
    REQUIRE(ringBuffer->readByte(byte));
    REQUIRE(byte == bytes.at(2));

    // check the empty ring
    REQUIRE_FALSE(ringBuffer->readByte(byte));
    REQUIRE(ringBuffer->getEmpty());
    REQUIRE(ringBuffer->getAvailableSize() == CommonSettings::RING_BUFFER_SIZE - 1);
}

TEST_CASE_METHOD(TestRingBuffer,
                 "A full ring keeps one byte free and refuses the next byte",
                 "[RingBuffer][testFull]")
{
    // fill the ring up to its capacity
    static constexpr uint8_t firstValue = 0;
    const std::vector<uint8_t> bytes = getCountingBytes(CommonSettings::RING_BUFFER_SIZE - 1,
                                                        firstValue);
    REQUIRE(ringBuffer->writeData(bytes.data(), bytes.size()));
    REQUIRE(ringBuffer->getAvailableSize() == static_cast<size_t>(0));

    // check that one more byte is refused
    REQUIRE_FALSE(ringBuffer->writeByte(firstValue));

    // read every byte back
    std::vector<uint8_t> readBytes(CommonSettings::RING_BUFFER_SIZE, 0);
    REQUIRE(ringBuffer->readData(readBytes.data(), readBytes.size()) == bytes.size());
    REQUIRE(std::equal(bytes.cbegin(), bytes.cend(), readBytes.cbegin()));
}

TEST_CASE_METHOD(TestRingBuffer,
                 "Data is written whole or not at all and read up to the asked size",
                 "[RingBuffer][testData]")
{
    // write data that does not fit after half of the ring is used
    static constexpr uint8_t firstValue = 1;
    const std::vector<uint8_t> halfBytes = getCountingBytes(CommonSettings::RING_BUFFER_SIZE / 2,
                                                            firstValue);
    REQUIRE(ringBuffer->writeData(halfBytes.data(), halfBytes.size()));
    REQUIRE_FALSE(ringBuffer->writeData(halfBytes.data(), halfBytes.size()));
    REQUIRE(ringBuffer->getAvailableSize() == CommonSettings::RING_BUFFER_SIZE / 2 - 1);

    // read fewer bytes than wait
    static constexpr size_t partSize = 5;
    std::vector<uint8_t> readBytes(partSize, 0);
    REQUIRE(ringBuffer->readData(readBytes.data(), readBytes.size()) == partSize);
    REQUIRE(readBytes == std::vector<uint8_t>(halfBytes.cbegin(), halfBytes.cbegin() + partSize));

    // check missing data
    REQUIRE_FALSE(ringBuffer->writeData(nullptr, partSize));
    REQUIRE(ringBuffer->writeData(nullptr, 0));
    REQUIRE(ringBuffer->readData(nullptr, partSize) == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestRingBuffer,
                 "Bytes keep their order when the indexes wrap around the end of the buffer",
                 "[RingBuffer][testWrap]")
{
    // move both indexes close to the end of the buffer
    static constexpr uint8_t firstValue = 7;
    const std::vector<uint8_t> fillBytes = getCountingBytes(CommonSettings::RING_BUFFER_SIZE - 2,
                                                            firstValue);
    std::vector<uint8_t> readBytes(CommonSettings::RING_BUFFER_SIZE, 0);
    REQUIRE(ringBuffer->writeData(fillBytes.data(), fillBytes.size()));
    REQUIRE(ringBuffer->readData(readBytes.data(), readBytes.size()) == fillBytes.size());

    // write and read data across the end of the buffer
    static constexpr size_t wrappedSize = 10;
    const std::vector<uint8_t> wrappedBytes = getCountingBytes(wrappedSize, firstValue);
    REQUIRE(ringBuffer->writeData(wrappedBytes.data(), wrappedBytes.size()));
    REQUIRE(ringBuffer->readData(readBytes.data(), readBytes.size()) == wrappedSize);
    readBytes.resize(wrappedSize);
    REQUIRE(readBytes == wrappedBytes);
    REQUIRE(ringBuffer->getEmpty());
}

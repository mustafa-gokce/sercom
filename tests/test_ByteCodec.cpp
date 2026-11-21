#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/ByteCodec.h"

using namespace sercom;

class TestByteCodec
{
public:
    /*
     * Byte codec test fixture.
     */
    TestByteCodec()
    {
        // initialize buffer
        static constexpr size_t bufferSize = 32;
        this->buffer.assign(bufferSize, 0);
    }
    ~TestByteCodec() = default;

    // delete copy and move operators
    TestByteCodec(const TestByteCodec &other) = delete;
    TestByteCodec &operator=(const TestByteCodec &other) = delete;
    TestByteCodec(TestByteCodec &&other) noexcept = delete;
    TestByteCodec &operator=(TestByteCodec &&other) noexcept = delete;

protected:
    std::vector<uint8_t> buffer; // zeroed buffer of the writers and readers
};

TEST_CASE_METHOD(TestByteCodec,
                 "Multi byte values are stored least significant byte first",
                 "[ByteCodec][testByteOrder]")
{
    // store a 16 bit value
    ByteCodec::storeUint16(buffer.data(), 0x1234);
    CAPTURE(buffer);
    REQUIRE(buffer.at(0) == 0x34);
    REQUIRE(buffer.at(1) == 0x12);

    // store a 32 bit value
    ByteCodec::storeUint32(buffer.data(), 0xA1B2C3D4);
    CAPTURE(buffer);
    REQUIRE(buffer.at(0) == 0xD4);
    REQUIRE(buffer.at(1) == 0xC3);
    REQUIRE(buffer.at(2) == 0xB2);
    REQUIRE(buffer.at(3) == 0xA1);

    // load both values again
    REQUIRE(ByteCodec::loadUint16(buffer.data()) == 0xC3D4);
    REQUIRE(ByteCodec::loadUint32(buffer.data()) == static_cast<uint32_t>(0xA1B2C3D4));
}

TEST_CASE_METHOD(TestByteCodec,
                 "Byte writer and byte reader round trip every supported type",
                 "[ByteCodec][testRoundTrip]")
{
    // write a value of every type
    const std::string text = "hi";
    ByteCodec::ByteWriter byteWriter(buffer.data(), buffer.size());
    byteWriter.writeUint8(0xFE);
    byteWriter.writeUint16(0xBEEF);
    byteWriter.writeUint32(0xDEADBEEF);
    byteWriter.writeInt16(-12345);
    byteWriter.writeInt32(std::numeric_limits<int32_t>::min());
    byteWriter.writeFloat32(-1.5F);
    byteWriter.writeString(text.data(), text.size());
    REQUIRE(byteWriter.getValid());
    REQUIRE(byteWriter.getSize()
            == sizeof(uint8_t) + sizeof(uint16_t) + sizeof(uint32_t) + sizeof(int16_t)
                   + sizeof(int32_t) + sizeof(float) + sizeof(uint8_t) + text.size());

    // read every value back
    static constexpr size_t readTextCapacity = 8;
    ByteCodec::ByteReader byteReader(buffer.data(), byteWriter.getSize());
    uint8_t uint8Value = 0;
    uint16_t uint16Value = 0;
    uint32_t uint32Value = 0;
    int16_t int16Value = 0;
    int32_t int32Value = 0;
    float float32Value = 0.0F;
    std::vector<char> readText(readTextCapacity);
    REQUIRE(byteReader.readUint8(uint8Value));
    REQUIRE(byteReader.readUint16(uint16Value));
    REQUIRE(byteReader.readUint32(uint32Value));
    REQUIRE(byteReader.readInt16(int16Value));
    REQUIRE(byteReader.readInt32(int32Value));
    REQUIRE(byteReader.readFloat32(float32Value));
    REQUIRE(byteReader.readString(readText.data(), readText.size()));

    // check the read values
    REQUIRE(uint8Value == 0xFE);
    REQUIRE(uint16Value == 0xBEEF);
    REQUIRE(uint32Value == static_cast<uint32_t>(0xDEADBEEF));
    REQUIRE(int16Value == -12345);
    REQUIRE(int32Value == std::numeric_limits<int32_t>::min());
    REQUIRE(float32Value == Catch::Approx(-1.5).margin(0.0));
    REQUIRE(std::string(readText.data()) == text);
    REQUIRE(byteReader.getRemainingSize() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestByteCodec,
                 "Signed values use two's complement on the wire",
                 "[ByteCodec][testSignedValue]")
{
    // write two negative values
    ByteCodec::ByteWriter byteWriter(buffer.data(), buffer.size());
    byteWriter.writeInt16(-1);
    byteWriter.writeInt32(-2);

    // check their two's complement bytes
    CAPTURE(buffer);
    REQUIRE(buffer.at(0) == 0xFF);
    REQUIRE(buffer.at(1) == 0xFF);
    REQUIRE(ByteCodec::loadUint32(buffer.data() + sizeof(int16_t))
            == static_cast<uint32_t>(0xFFFFFFFE));
}

TEST_CASE_METHOD(TestByteCodec,
                 "Floating point values travel as their IEEE 754 bit pattern",
                 "[ByteCodec][testFloatValue]")
{
    // write a floating point value
    ByteCodec::ByteWriter byteWriter(buffer.data(), buffer.size());
    byteWriter.writeFloat32(1.0F);

    // check its bit pattern
    REQUIRE(ByteCodec::loadUint32(buffer.data()) == static_cast<uint32_t>(0x3F800000));
}

TEST_CASE_METHOD(TestByteCodec,
                 "A write that does not fit invalidates the byte writer and changes nothing",
                 "[ByteCodec][testWriteOverflow]")
{
    // write a value that fits
    static constexpr size_t byteWriterCapacity = 5;
    ByteCodec::ByteWriter byteWriter(buffer.data(), byteWriterCapacity);
    byteWriter.writeUint32(0x11223344);
    REQUIRE(byteWriter.getValid());

    // write a value that does not fit
    byteWriter.writeUint16(0xAAAA);
    REQUIRE_FALSE(byteWriter.getValid());
    REQUIRE(byteWriter.getSize() == sizeof(uint32_t));
    REQUIRE(buffer.at(4) == 0x00);

    // write a value that fits again (the invalid state stays)
    byteWriter.writeUint8(0x55);
    REQUIRE_FALSE(byteWriter.getValid());
    REQUIRE(buffer.at(4) == 0x00);
}

TEST_CASE_METHOD(TestByteCodec,
                 "A read past the end invalidates the byte reader",
                 "[ByteCodec][testReadOverflow]")
{
    // read a present value and a missing value
    const std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    ByteCodec::ByteReader byteReader(data.data(), data.size());
    uint16_t uint16Value = 0;
    uint32_t uint32Value = 0;
    uint8_t uint8Value = 0;
    REQUIRE(byteReader.readUint16(uint16Value));
    REQUIRE_FALSE(byteReader.readUint32(uint32Value));
    REQUIRE_FALSE(byteReader.getValid());

    // read the remaining byte (the invalid state stays)
    REQUIRE_FALSE(byteReader.readUint8(uint8Value));
    REQUIRE(byteReader.getPosition() == sizeof(uint16_t));
}

TEST_CASE_METHOD(TestByteCodec,
                 "Strings longer than the destination are rejected",
                 "[ByteCodec][testLongString]")
{
    // write a string
    const std::string text = "hello";
    ByteCodec::ByteWriter byteWriter(buffer.data(), buffer.size());
    byteWriter.writeString(text.data(), text.size());
    REQUIRE(byteWriter.getValid());

    // read the string without room for its null character
    ByteCodec::ByteReader byteReader(buffer.data(), byteWriter.getSize());
    std::vector<char> readText(text.size());
    REQUIRE_FALSE(byteReader.readString(readText.data(), readText.size()));
    REQUIRE_FALSE(byteReader.getValid());
}

TEST_CASE_METHOD(TestByteCodec,
                 "Strings announcing more bytes than present are rejected",
                 "[ByteCodec][testTruncatedString]")
{
    // read a string that announces ten characters but carries two
    static constexpr size_t readTextCapacity = 16;
    const std::vector<uint8_t> data = {10, 'a', 'b'};
    ByteCodec::ByteReader byteReader(data.data(), data.size());
    std::vector<char> readText(readTextCapacity);
    REQUIRE_FALSE(byteReader.readString(readText.data(), readText.size()));
}

TEST_CASE_METHOD(TestByteCodec,
                 "Strings above 255 characters cannot be written",
                 "[ByteCodec][testOversizedString]")
{
    // write a string one character above its length field
    static constexpr size_t largeBufferSize = 300;
    const std::string text(static_cast<size_t>(std::numeric_limits<uint8_t>::max()) + 1, 'x');
    std::vector<uint8_t> largeBuffer(largeBufferSize);
    ByteCodec::ByteWriter byteWriter(largeBuffer.data(), largeBuffer.size());
    byteWriter.writeString(text.data(), text.size());
    REQUIRE_FALSE(byteWriter.getValid());
}

TEST_CASE_METHOD(TestByteCodec, "Null buffers never crash", "[ByteCodec][testNullBuffer]")
{
    // write into a missing buffer
    static constexpr size_t missingBufferSize = 100;
    ByteCodec::ByteWriter byteWriter(nullptr, missingBufferSize);
    byteWriter.writeUint8(1);
    REQUIRE_FALSE(byteWriter.getValid());

    // read from a missing buffer
    ByteCodec::ByteReader byteReader(nullptr, missingBufferSize);
    uint8_t uint8Value = 0;
    REQUIRE_FALSE(byteReader.readUint8(uint8Value));
}

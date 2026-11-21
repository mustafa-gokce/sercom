#include <cstdint>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"

using namespace sercom;

class TestDefinitions
{
public:
    /*
     * Definitions test fixture.
     */
    TestDefinitions() = default;
    ~TestDefinitions() = default;

    // delete copy and move operators
    TestDefinitions(const TestDefinitions &other) = delete;
    TestDefinitions &operator=(const TestDefinitions &other) = delete;
    TestDefinitions(TestDefinitions &&other) noexcept = delete;
    TestDefinitions &operator=(TestDefinitions &&other) noexcept = delete;
};

TEST_CASE_METHOD(TestDefinitions,
                 "Frame flag type can be converted to integer",
                 "[Definitions][fromFrameFlagType]")
{
    REQUIRE(Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK_REQUEST) == 0x01);
    REQUIRE(Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK) == 0x02);
    REQUIRE(Definitions::fromFrameFlagType(Definitions::FrameFlagType::NACK) == 0x04);
    REQUIRE(Definitions::fromFrameFlagType(Definitions::FrameFlagType::RETRANSMISSION) == 0x08);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Frame flag type can be converted from integer",
                 "[Definitions][toFrameFlagType]")
{
    REQUIRE(Definitions::toFrameFlagType(0x01) == Definitions::FrameFlagType::ACK_REQUEST);
    REQUIRE(Definitions::toFrameFlagType(0x02) == Definitions::FrameFlagType::ACK);
    REQUIRE(Definitions::toFrameFlagType(0x04) == Definitions::FrameFlagType::NACK);
    REQUIRE(Definitions::toFrameFlagType(0x08) == Definitions::FrameFlagType::RETRANSMISSION);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Decode error type can be converted to string",
                 "[Definitions][fromDecodeErrorType]")
{
    REQUIRE(Definitions::fromDecodeErrorType(Definitions::DecodeErrorType::UNKNOWN) == "UNKNOWN");
    REQUIRE(Definitions::fromDecodeErrorType(Definitions::DecodeErrorType::INVALID_VERSION)
            == "INVALID_VERSION");
    REQUIRE(Definitions::fromDecodeErrorType(Definitions::DecodeErrorType::PAYLOAD_TOO_LONG)
            == "PAYLOAD_TOO_LONG");
    REQUIRE(Definitions::fromDecodeErrorType(Definitions::DecodeErrorType::CRC_MISMATCH)
            == "CRC_MISMATCH");
    REQUIRE(Definitions::fromDecodeErrorType(Definitions::DecodeErrorType::INTER_BYTE_TIMEOUT)
            == "INTER_BYTE_TIMEOUT");
}

TEST_CASE_METHOD(TestDefinitions,
                 "Decode error type can be parsed from string",
                 "[Definitions][toDecodeErrorType]")
{
    REQUIRE(Definitions::toDecodeErrorType("UNKNOWN") == Definitions::DecodeErrorType::UNKNOWN);
    REQUIRE(Definitions::toDecodeErrorType("INVALID_VERSION")
            == Definitions::DecodeErrorType::INVALID_VERSION);
    REQUIRE(Definitions::toDecodeErrorType("PAYLOAD_TOO_LONG")
            == Definitions::DecodeErrorType::PAYLOAD_TOO_LONG);
    REQUIRE(Definitions::toDecodeErrorType("CRC_MISMATCH")
            == Definitions::DecodeErrorType::CRC_MISMATCH);
    REQUIRE(Definitions::toDecodeErrorType("INTER_BYTE_TIMEOUT")
            == Definitions::DecodeErrorType::INTER_BYTE_TIMEOUT);
    REQUIRE(Definitions::toDecodeErrorType("LOREM_IPSUM") == Definitions::DecodeErrorType::UNKNOWN);
    REQUIRE(Definitions::toDecodeErrorType("invalid_version")
            == Definitions::DecodeErrorType::UNKNOWN);
    REQUIRE(Definitions::toDecodeErrorType("INVALID_VERSION ")
            == Definitions::DecodeErrorType::UNKNOWN);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Frame flags can be checked in the value of a flags field",
                 "[Definitions][testFrameFlagSet]")
{
    // check the flags of a combined value
    static constexpr uint8_t flags = 0x09;
    REQUIRE(Definitions::isFrameFlagSet(flags, Definitions::FrameFlagType::ACK_REQUEST));
    REQUIRE_FALSE(Definitions::isFrameFlagSet(flags, Definitions::FrameFlagType::ACK));
    REQUIRE_FALSE(Definitions::isFrameFlagSet(flags, Definitions::FrameFlagType::NACK));
    REQUIRE(Definitions::isFrameFlagSet(flags, Definitions::FrameFlagType::RETRANSMISSION));

    // check the flags of an empty value
    REQUIRE_FALSE(Definitions::isFrameFlagSet(0, Definitions::FrameFlagType::ACK_REQUEST));
    REQUIRE_FALSE(Definitions::isFrameFlagSet(0, Definitions::FrameFlagType::RETRANSMISSION));
}

TEST_CASE_METHOD(TestDefinitions,
                 "Frame flag type is shown by its name",
                 "[Definitions][getFrameFlagTypeText]")
{
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::FrameFlagType::ACK_REQUEST)
            == "ACK_REQUEST");
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::FrameFlagType::ACK) == "ACK");
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::FrameFlagType::NACK) == "NACK");
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::FrameFlagType::RETRANSMISSION)
            == "RETRANSMISSION");
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::toFrameFlagType(0x10)) == "UNKNOWN");
}

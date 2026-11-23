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
    REQUIRE(Definitions::fromFrameFlagType(Definitions::FrameFlagType::ROUTE_CAPABLE) == 0x10);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Frame flag type can be converted from integer",
                 "[Definitions][toFrameFlagType]")
{
    REQUIRE(Definitions::toFrameFlagType(0x01) == Definitions::FrameFlagType::ACK_REQUEST);
    REQUIRE(Definitions::toFrameFlagType(0x02) == Definitions::FrameFlagType::ACK);
    REQUIRE(Definitions::toFrameFlagType(0x04) == Definitions::FrameFlagType::NACK);
    REQUIRE(Definitions::toFrameFlagType(0x08) == Definitions::FrameFlagType::RETRANSMISSION);
    REQUIRE(Definitions::toFrameFlagType(0x10) == Definitions::FrameFlagType::ROUTE_CAPABLE);
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
                 "Message status type can be converted to integer",
                 "[Definitions][fromMessageStatusType]")
{
    REQUIRE(Definitions::fromMessageStatusType(Definitions::MessageStatusType::UNKNOWN) == 0);
    REQUIRE(Definitions::fromMessageStatusType(Definitions::MessageStatusType::ACCEPTED) == 1);
    REQUIRE(
        Definitions::fromMessageStatusType(Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE)
        == 2);
    REQUIRE(Definitions::fromMessageStatusType(Definitions::MessageStatusType::MALFORMED_PAYLOAD)
            == 3);
    REQUIRE(Definitions::fromMessageStatusType(Definitions::MessageStatusType::BUSY) == 4);
    REQUIRE(Definitions::fromMessageStatusType(Definitions::MessageStatusType::INVALID_ARGUMENT)
            == 5);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Message status type can be converted from integer",
                 "[Definitions][toMessageStatusType]")
{
    REQUIRE(Definitions::toMessageStatusType(0) == Definitions::MessageStatusType::UNKNOWN);
    REQUIRE(Definitions::toMessageStatusType(1) == Definitions::MessageStatusType::ACCEPTED);
    REQUIRE(Definitions::toMessageStatusType(2)
            == Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE);
    REQUIRE(Definitions::toMessageStatusType(3)
            == Definitions::MessageStatusType::MALFORMED_PAYLOAD);
    REQUIRE(Definitions::toMessageStatusType(4) == Definitions::MessageStatusType::BUSY);
    REQUIRE(Definitions::toMessageStatusType(5) == Definitions::MessageStatusType::INVALID_ARGUMENT);
    REQUIRE(Definitions::toMessageStatusType(6) == Definitions::MessageStatusType::UNKNOWN);
    REQUIRE(Definitions::toMessageStatusType(200) == Definitions::MessageStatusType::UNKNOWN);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Send status type can be converted to string",
                 "[Definitions][fromSendStatusType]")
{
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::UNKNOWN) == "UNKNOWN");
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::SENT) == "SENT");
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::WINDOW_FULL)
            == "WINDOW_FULL");
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::PAYLOAD_TOO_LONG)
            == "PAYLOAD_TOO_LONG");
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::TRANSPORT_ERROR)
            == "TRANSPORT_ERROR");
    REQUIRE(Definitions::fromSendStatusType(Definitions::SendStatusType::INVALID_ARGUMENT)
            == "INVALID_ARGUMENT");
}

TEST_CASE_METHOD(TestDefinitions,
                 "Send status type can be parsed from string",
                 "[Definitions][toSendStatusType]")
{
    REQUIRE(Definitions::toSendStatusType("UNKNOWN") == Definitions::SendStatusType::UNKNOWN);
    REQUIRE(Definitions::toSendStatusType("SENT") == Definitions::SendStatusType::SENT);
    REQUIRE(Definitions::toSendStatusType("WINDOW_FULL")
            == Definitions::SendStatusType::WINDOW_FULL);
    REQUIRE(Definitions::toSendStatusType("PAYLOAD_TOO_LONG")
            == Definitions::SendStatusType::PAYLOAD_TOO_LONG);
    REQUIRE(Definitions::toSendStatusType("TRANSPORT_ERROR")
            == Definitions::SendStatusType::TRANSPORT_ERROR);
    REQUIRE(Definitions::toSendStatusType("INVALID_ARGUMENT")
            == Definitions::SendStatusType::INVALID_ARGUMENT);
    REQUIRE(Definitions::toSendStatusType("LOREM_IPSUM") == Definitions::SendStatusType::UNKNOWN);
    REQUIRE(Definitions::toSendStatusType("sent") == Definitions::SendStatusType::UNKNOWN);
    REQUIRE(Definitions::toSendStatusType("SENT ") == Definitions::SendStatusType::UNKNOWN);
}

TEST_CASE_METHOD(TestDefinitions,
                 "Delivery failure type can be converted to string",
                 "[Definitions][fromDeliveryFailureType]")
{
    REQUIRE(Definitions::fromDeliveryFailureType(Definitions::DeliveryFailureType::UNKNOWN)
            == "UNKNOWN");
    REQUIRE(Definitions::fromDeliveryFailureType(Definitions::DeliveryFailureType::TIMED_OUT)
            == "TIMED_OUT");
    REQUIRE(Definitions::fromDeliveryFailureType(Definitions::DeliveryFailureType::REJECTED)
            == "REJECTED");
}

TEST_CASE_METHOD(TestDefinitions,
                 "Delivery failure type can be parsed from string",
                 "[Definitions][toDeliveryFailureType]")
{
    REQUIRE(Definitions::toDeliveryFailureType("UNKNOWN")
            == Definitions::DeliveryFailureType::UNKNOWN);
    REQUIRE(Definitions::toDeliveryFailureType("TIMED_OUT")
            == Definitions::DeliveryFailureType::TIMED_OUT);
    REQUIRE(Definitions::toDeliveryFailureType("REJECTED")
            == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(Definitions::toDeliveryFailureType("LOREM_IPSUM")
            == Definitions::DeliveryFailureType::UNKNOWN);
    REQUIRE(Definitions::toDeliveryFailureType("timed_out")
            == Definitions::DeliveryFailureType::UNKNOWN);
    REQUIRE(Definitions::toDeliveryFailureType("TIMED_OUT ")
            == Definitions::DeliveryFailureType::UNKNOWN);
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
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::FrameFlagType::ROUTE_CAPABLE)
            == "ROUTE_CAPABLE");
    REQUIRE(Definitions::getFrameFlagTypeText(Definitions::toFrameFlagType(0x20)) == "UNKNOWN");
}

TEST_CASE_METHOD(TestDefinitions,
                 "A routed frame keeps the direct header and adds three routing bytes",
                 "[Definitions][testRoutedFrameSizes]")
{
    // check the versions of both frame formats
    REQUIRE(Definitions::PROTOCOL_VERSION == 1);
    REQUIRE(Definitions::ROUTED_PROTOCOL_VERSION == 2);

    // check the sizes of the routed frame
    REQUIRE(Definitions::ROUTE_HEADER_SIZE == static_cast<size_t>(3));
    REQUIRE(Definitions::ROUTED_HEADER_SIZE
            == Definitions::HEADER_SIZE + Definitions::ROUTE_HEADER_SIZE);
    REQUIRE(Definitions::ROUTED_HEADER_SIZE == static_cast<size_t>(12));
    REQUIRE(Definitions::ROUTED_FRAME_OVERHEAD == static_cast<size_t>(16));

    // check the addresses
    REQUIRE(Definitions::UNASSIGNED_ADDRESS == 0x00);
    REQUIRE(Definitions::HOST_ADDRESS == 0x01);
    REQUIRE(Definitions::MINIMUM_NODE_ADDRESS == 0x02);
    REQUIRE(Definitions::MAXIMUM_NODE_ADDRESS == 0xFE);
    REQUIRE(Definitions::BROADCAST_ADDRESS == 0xFF);
}

TEST_CASE_METHOD(TestDefinitions,
                 "The header size follows the version of a frame",
                 "[Definitions][getHeaderSize]")
{
    // check the header sizes of both versions
    REQUIRE(Definitions::getHeaderSize(Definitions::PROTOCOL_VERSION) == Definitions::HEADER_SIZE);
    REQUIRE(Definitions::getHeaderSize(Definitions::ROUTED_PROTOCOL_VERSION)
            == Definitions::ROUTED_HEADER_SIZE);

    // check that an unknown version has no header size
    REQUIRE(Definitions::getHeaderSize(0) == static_cast<size_t>(0));
    REQUIRE(Definitions::getHeaderSize(Definitions::ROUTED_PROTOCOL_VERSION + 1)
            == static_cast<size_t>(0));
    REQUIRE(Definitions::getHeaderSize(0xFF) == static_cast<size_t>(0));
}

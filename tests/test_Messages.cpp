#include <chrono>
#include <cstdint>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../app/include/Messages.h"
#include "../core/include/Frame.h"
#include "../core/include/Settings.h"

using namespace std::chrono_literals;
using namespace sercom;

class TestMessages
{
public:
    /*
     * Messages test fixture.
     */
    TestMessages() = default;
    ~TestMessages() = default;

    /*
     * Get the payload bytes of a message.
     */
    [[nodiscard]] static std::vector<uint8_t> getPayload(
        const Messages::GenericMessage &genericMessage)
    {
        // expose a copy of the payload bytes
        return std::vector<uint8_t>(genericMessage.getPayload(),
                                    genericMessage.getPayload() + genericMessage.getPayloadLength());
    }

    /*
     * Get a frame that carries a payload.
     */
    [[nodiscard]] static Frame::Frame getFrame(const std::vector<uint8_t> &payload)
    {
        // expose a frame with the payload
        return Frame::Frame(0, 0, 0, payload.data(), payload.size());
    }

    // delete copy and move operators
    TestMessages(const TestMessages &other) = delete;
    TestMessages &operator=(const TestMessages &other) = delete;
    TestMessages(TestMessages &&other) noexcept = delete;
    TestMessages &operator=(TestMessages &&other) noexcept = delete;
};

TEST_CASE_METHOD(TestMessages,
                 "Message type can be converted to integer",
                 "[Messages][fromMessageType]")
{
    REQUIRE(Messages::fromMessageType(Messages::MessageType::UNKNOWN) == 0x0000);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::PING_REQUEST) == 0x0001);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::PING_RESPONSE) == 0x0002);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::DEVICE_INFO_REQUEST) == 0x0003);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::DEVICE_INFO_RESPONSE) == 0x0004);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::LOG_REPORT_RESPONSE) == 0x0005);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::ECHO_REQUEST) == 0x0006);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::ECHO_RESPONSE) == 0x0007);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST) == 0x0100);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::SET_LED_RESPONSE) == 0x0101);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST)
            == 0x0102);
    REQUIRE(Messages::fromMessageType(Messages::MessageType::TELEMETRY_REPORT_RESPONSE) == 0x0103);
}

TEST_CASE_METHOD(TestMessages,
                 "Message type can be converted from integer",
                 "[Messages][toMessageType]")
{
    REQUIRE(Messages::toMessageType(0x0000) == Messages::MessageType::UNKNOWN);
    REQUIRE(Messages::toMessageType(0x0001) == Messages::MessageType::PING_REQUEST);
    REQUIRE(Messages::toMessageType(0x0002) == Messages::MessageType::PING_RESPONSE);
    REQUIRE(Messages::toMessageType(0x0003) == Messages::MessageType::DEVICE_INFO_REQUEST);
    REQUIRE(Messages::toMessageType(0x0004) == Messages::MessageType::DEVICE_INFO_RESPONSE);
    REQUIRE(Messages::toMessageType(0x0005) == Messages::MessageType::LOG_REPORT_RESPONSE);
    REQUIRE(Messages::toMessageType(0x0006) == Messages::MessageType::ECHO_REQUEST);
    REQUIRE(Messages::toMessageType(0x0007) == Messages::MessageType::ECHO_RESPONSE);
    REQUIRE(Messages::toMessageType(0x0100) == Messages::MessageType::SET_LED_REQUEST);
    REQUIRE(Messages::toMessageType(0x0101) == Messages::MessageType::SET_LED_RESPONSE);
    REQUIRE(Messages::toMessageType(0x0102) == Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST);
    REQUIRE(Messages::toMessageType(0x0103) == Messages::MessageType::TELEMETRY_REPORT_RESPONSE);
    REQUIRE(Messages::toMessageType(0x4242) == Messages::MessageType::UNKNOWN);
    REQUIRE(Messages::toMessageType(0xFFFF) == Messages::MessageType::UNKNOWN);
}

TEST_CASE_METHOD(TestMessages,
                 "Log level type can be converted to integer",
                 "[Messages][fromLogLevelType]")
{
    REQUIRE(Messages::fromLogLevelType(Messages::LogLevelType::UNKNOWN) == 0);
    REQUIRE(Messages::fromLogLevelType(Messages::LogLevelType::DEBUG) == 1);
    REQUIRE(Messages::fromLogLevelType(Messages::LogLevelType::WARNING) == 2);
}

TEST_CASE_METHOD(TestMessages,
                 "Log level type can be converted from integer",
                 "[Messages][toLogLevelType]")
{
    REQUIRE(Messages::toLogLevelType(0) == Messages::LogLevelType::UNKNOWN);
    REQUIRE(Messages::toLogLevelType(1) == Messages::LogLevelType::DEBUG);
    REQUIRE(Messages::toLogLevelType(2) == Messages::LogLevelType::WARNING);
    REQUIRE(Messages::toLogLevelType(3) == Messages::LogLevelType::UNKNOWN);
    REQUIRE(Messages::toLogLevelType(200) == Messages::LogLevelType::UNKNOWN);
}

TEST_CASE_METHOD(TestMessages, "Message type is shown by its name", "[Messages][getMessageTypeText]")
{
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::UNKNOWN) == "UNKNOWN");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::PING_REQUEST) == "PING_REQUEST");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::PING_RESPONSE) == "PING_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::DEVICE_INFO_REQUEST)
            == "DEVICE_INFO_REQUEST");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::DEVICE_INFO_RESPONSE)
            == "DEVICE_INFO_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::LOG_REPORT_RESPONSE)
            == "LOG_REPORT_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::ECHO_REQUEST) == "ECHO_REQUEST");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::ECHO_RESPONSE) == "ECHO_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::SET_LED_REQUEST)
            == "SET_LED_REQUEST");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::SET_LED_RESPONSE)
            == "SET_LED_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST)
            == "SET_TELEMETRY_PERIOD_REQUEST");
    REQUIRE(Messages::getMessageTypeText(Messages::MessageType::TELEMETRY_REPORT_RESPONSE)
            == "TELEMETRY_REPORT_RESPONSE");
    REQUIRE(Messages::getMessageTypeText(Messages::toMessageType(0x4242)) == "UNKNOWN");
}

TEST_CASE_METHOD(TestMessages,
                 "Log level type is shown by its name",
                 "[Messages][getLogLevelTypeText]")
{
    REQUIRE(Messages::getLogLevelTypeText(Messages::LogLevelType::UNKNOWN) == "UNKNOWN");
    REQUIRE(Messages::getLogLevelTypeText(Messages::LogLevelType::DEBUG) == "DEBUG");
    REQUIRE(Messages::getLogLevelTypeText(Messages::LogLevelType::WARNING) == "WARNING");
    REQUIRE(Messages::getLogLevelTypeText(Messages::toLogLevelType(3)) == "UNKNOWN");
}

TEST_CASE_METHOD(TestMessages,
                 "Ping request keeps the lower 32 bits of its request timestamp",
                 "[Messages][testPingRequestTimestampWidth]")
{
    // create a ping request beyond the 32 bit milliseconds
    const Messages::PingRequest pingRequest(4294967301ms);

    // check the request timestamp of its field
    REQUIRE(pingRequest.getPayloadLength() == Messages::PING_REQUEST_SIZE);
    REQUIRE(pingRequest.getRequestTimestamp() == 5ms);
}

TEST_CASE_METHOD(TestMessages,
                 "Device info request has no payload",
                 "[Messages][testDeviceInfoRequest]")
{
    // create a device info request
    const Messages::DeviceInfoRequest deviceInfoRequest;

    // check the empty payload
    REQUIRE(deviceInfoRequest.getPayloadLength() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestMessages,
                 "Echo request keeps its data and rejects data above the payload",
                 "[Messages][testEchoRequest]")
{
    // create an echo request
    const std::vector<uint8_t> data = {0xA5, 0xC3, 0x00, 0xFF, 0x42};
    const Messages::EchoRequest echoRequest(data.data(), data.size());

    // check the echo request of a received frame
    const Messages::EchoRequest receivedEchoRequest(getFrame(getPayload(echoRequest)));
    REQUIRE(getPayload(receivedEchoRequest) == data);

    // check the echo response with the echo request payload
    const Messages::EchoResponse echoResponse(receivedEchoRequest.getPayload(),
                                              receivedEchoRequest.getPayloadLength());
    REQUIRE(getPayload(echoResponse) == data);

    // check the empty echo request with too much data
    const std::vector<uint8_t> longData(Settings::MaximumPayloadSize + 1, 0x55);
    const Messages::EchoRequest longEchoRequest(longData.data(), longData.size());
    REQUIRE(longEchoRequest.getPayloadLength() == static_cast<size_t>(0));
}

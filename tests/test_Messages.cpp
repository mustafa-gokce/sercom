#include <chrono>
#include <cstdint>
#include <string>
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
                 "Ping request keeps its request timestamp",
                 "[Messages][testPingRequest]")
{
    // create a ping request
    const Messages::PingRequest pingRequest(123456789ms);
    const std::vector<uint8_t> payload = getPayload(pingRequest);

    // check the little-endian payload
    CAPTURE(payload);
    REQUIRE(payload == std::vector<uint8_t>({0x15, 0xCD, 0x5B, 0x07}));
    REQUIRE(pingRequest.getLikelyValid());
    REQUIRE(pingRequest.getRequestTimestamp() == 123456789ms);

    // check the ping request of a received frame
    const Messages::PingRequest receivedPingRequest(getFrame(payload));
    REQUIRE(receivedPingRequest.getLikelyValid());
    REQUIRE(receivedPingRequest.getRequestTimestamp() == 123456789ms);
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
                 "Ping response keeps its request timestamp and the uptime",
                 "[Messages][testPingResponse]")
{
    // create a ping response
    const Messages::PingResponse pingResponse(1000ms, 2ms);
    const std::vector<uint8_t> payload = getPayload(pingResponse);

    // check the payload
    CAPTURE(payload);
    REQUIRE(payload == std::vector<uint8_t>({0xE8, 0x03, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}));

    // check the ping response of a received frame
    const Messages::PingResponse receivedPingResponse(getFrame(payload));
    REQUIRE(receivedPingResponse.getLikelyValid());
    REQUIRE(receivedPingResponse.getRequestTimestamp() == 1000ms);
    REQUIRE(receivedPingResponse.getUptime() == 2ms);
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
                 "Device info response keeps every field and the device name",
                 "[Messages][testDeviceInfoResponse]")
{
    // create a device info response
    const std::string deviceName = "bench-unit-7";
    const Messages::DeviceInfoResponse deviceInfoResponse(1, 2, 123456ms, 200000, deviceName);
    const std::vector<uint8_t> payload = getPayload(deviceInfoResponse);

    // check the payload size
    CAPTURE(payload);
    REQUIRE(payload.size() == Messages::DEVICE_INFO_RESPONSE_SIZE + deviceName.size());
    REQUIRE(deviceInfoResponse.getLikelyValid());

    // check the device info response of a received frame
    const Messages::DeviceInfoResponse receivedDeviceInfoResponse(getFrame(payload));
    REQUIRE(receivedDeviceInfoResponse.getLikelyValid());
    REQUIRE(receivedDeviceInfoResponse.getFirmwareVersionMajor() == 1);
    REQUIRE(receivedDeviceInfoResponse.getFirmwareVersionMinor() == 2);
    REQUIRE(receivedDeviceInfoResponse.getUptime() == 123456ms);
    REQUIRE(receivedDeviceInfoResponse.getFreeHeapSize() == static_cast<uint32_t>(200000));
    REQUIRE(receivedDeviceInfoResponse.getDeviceName() == deviceName);
}

TEST_CASE_METHOD(TestMessages,
                 "Device info response with a device name above the payload stays empty",
                 "[Messages][testDeviceInfoResponseTooLong]")
{
    // create a device info response with an oversized device name
    const std::string deviceName(Settings::MaximumPayloadSize, 'x');
    const Messages::DeviceInfoResponse deviceInfoResponse(1, 2, 3ms, 4, deviceName);

    // check the empty device info response
    REQUIRE(deviceInfoResponse.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(deviceInfoResponse.getLikelyValid());
    REQUIRE(deviceInfoResponse.getDeviceName().empty());
}

TEST_CASE_METHOD(TestMessages,
                 "Device info response with a device name above the maximum length stays empty",
                 "[Messages][testDeviceInfoResponseNameLimit]")
{
    // longest device name must fit into a payload
    if (Messages::DEVICE_INFO_RESPONSE_SIZE + Messages::MAXIMUM_DEVICE_NAME_LENGTH
        > Settings::MaximumPayloadSize) {
        SKIP("The longest device name does not fit into a payload of this build");
    }

    // create device info responses at and above the maximum device name length
    const std::string deviceName(Messages::MAXIMUM_DEVICE_NAME_LENGTH, 'x');
    const std::string longDeviceName(Messages::MAXIMUM_DEVICE_NAME_LENGTH + 1, 'x');
    const Messages::DeviceInfoResponse deviceInfoResponse(1, 2, 3ms, 4, deviceName);
    const Messages::DeviceInfoResponse longDeviceInfoResponse(1, 2, 3ms, 4, longDeviceName);

    // check the device info response with the longest device name
    REQUIRE(deviceInfoResponse.getLikelyValid());
    REQUIRE(deviceInfoResponse.getDeviceName() == deviceName);

    // check the empty device info response above the maximum device name length
    REQUIRE(longDeviceInfoResponse.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(longDeviceInfoResponse.getLikelyValid());
    REQUIRE(longDeviceInfoResponse.getDeviceName().empty());
}

TEST_CASE_METHOD(TestMessages,
                 "Device info response with a cut device name is not likely valid",
                 "[Messages][testDeviceInfoResponseCutName]")
{
    // cut the device name of a device info response
    const Messages::DeviceInfoResponse deviceInfoResponse(1, 2, 3ms, 4, "bench-unit-7");
    std::vector<uint8_t> payload = getPayload(deviceInfoResponse);
    payload.pop_back();

    // check the device info response of a received frame
    const Messages::DeviceInfoResponse receivedDeviceInfoResponse(getFrame(payload));
    REQUIRE_FALSE(receivedDeviceInfoResponse.getLikelyValid());
    REQUIRE(receivedDeviceInfoResponse.getFirmwareVersionMajor() == 1);
    REQUIRE(receivedDeviceInfoResponse.getDeviceName().empty());
}

TEST_CASE_METHOD(TestMessages,
                 "Log report response keeps its log level type and its text",
                 "[Messages][testLogReportResponse]")
{
    // create a log report response
    const Messages::LogReportResponse logReportResponse(Messages::LogLevelType::WARNING,
                                                        "battery low");
    std::vector<uint8_t> payload = getPayload(logReportResponse);

    // check the log report response of a received frame
    const Messages::LogReportResponse receivedLogReportResponse(getFrame(payload));
    REQUIRE(receivedLogReportResponse.getLikelyValid());
    REQUIRE(receivedLogReportResponse.getLogLevelType() == Messages::LogLevelType::WARNING);
    REQUIRE(receivedLogReportResponse.getText() == "battery low");

    // check an unknown log level type of a received frame
    payload.at(0) = 200;
    const Messages::LogReportResponse unknownLogReportResponse(getFrame(payload));
    REQUIRE(unknownLogReportResponse.getLikelyValid());
    REQUIRE(unknownLogReportResponse.getLogLevelType() == Messages::LogLevelType::UNKNOWN);
}

TEST_CASE_METHOD(TestMessages,
                 "Log report response with a text above the payload stays empty",
                 "[Messages][testLogReportResponseTooLong]")
{
    // create a log report response with an oversized text
    const std::string text(Settings::MaximumPayloadSize, 'x');
    const Messages::LogReportResponse logReportResponse(Messages::LogLevelType::DEBUG, text);

    // check the empty log report response
    REQUIRE(logReportResponse.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(logReportResponse.getLikelyValid());
    REQUIRE(logReportResponse.getText().empty());
}

TEST_CASE_METHOD(TestMessages,
                 "Log report response with a text above the maximum length stays empty",
                 "[Messages][testLogReportResponseTextLimit]")
{
    // longest log text must fit into a payload
    if (Messages::LOG_REPORT_RESPONSE_SIZE + Messages::MAXIMUM_LOG_TEXT_LENGTH
        > Settings::MaximumPayloadSize) {
        SKIP("The longest log text does not fit into a payload of this build");
    }

    // create log report responses at and above the maximum log text length
    const std::string text(Messages::MAXIMUM_LOG_TEXT_LENGTH, 'x');
    const std::string longText(Messages::MAXIMUM_LOG_TEXT_LENGTH + 1, 'x');
    const Messages::LogReportResponse logReportResponse(Messages::LogLevelType::DEBUG, text);
    const Messages::LogReportResponse longLogReportResponse(Messages::LogLevelType::DEBUG, longText);

    // check the log report response with the longest text
    REQUIRE(logReportResponse.getLikelyValid());
    REQUIRE(logReportResponse.getText() == text);

    // check the empty log report response above the maximum log text length
    REQUIRE(longLogReportResponse.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(longLogReportResponse.getLikelyValid());
    REQUIRE(longLogReportResponse.getText().empty());
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

TEST_CASE_METHOD(TestMessages,
                 "Set LED request accepts only zero and one",
                 "[Messages][testSetLedRequest]")
{
    // create a set LED request
    const Messages::SetLedRequest setLedRequest(true);
    std::vector<uint8_t> payload = getPayload(setLedRequest);

    // check the payload
    CAPTURE(payload);
    REQUIRE(payload == std::vector<uint8_t>({0x01}));
    REQUIRE(setLedRequest.getLedOn());

    // check the set LED request of a received frame
    payload.at(0) = 0;
    const Messages::SetLedRequest switchedOffSetLedRequest(getFrame(payload));
    REQUIRE(switchedOffSetLedRequest.getLikelyValid());
    REQUIRE_FALSE(switchedOffSetLedRequest.getLedOn());

    // check a malformed LED state of a received frame
    payload.at(0) = 2;
    const Messages::SetLedRequest malformedSetLedRequest(getFrame(payload));
    REQUIRE_FALSE(malformedSetLedRequest.getLikelyValid());
}

TEST_CASE_METHOD(TestMessages,
                 "Set LED response accepts only zero and one",
                 "[Messages][testSetLedResponse]")
{
    // create a set LED response
    const Messages::SetLedResponse setLedResponse(false);
    std::vector<uint8_t> payload = getPayload(setLedResponse);

    // check the set LED response of a received frame
    const Messages::SetLedResponse receivedSetLedResponse(getFrame(payload));
    REQUIRE(receivedSetLedResponse.getLikelyValid());
    REQUIRE_FALSE(receivedSetLedResponse.getLedOn());

    // check a malformed LED state of a received frame
    payload.at(0) = 7;
    const Messages::SetLedResponse malformedSetLedResponse(getFrame(payload));
    REQUIRE_FALSE(malformedSetLedResponse.getLikelyValid());
}

TEST_CASE_METHOD(TestMessages,
                 "Set telemetry period request keeps its telemetry period",
                 "[Messages][testSetTelemetryPeriodRequest]")
{
    // create a set telemetry period request with the longest period
    const Messages::SetTelemetryPeriodRequest setTelemetryPeriodRequest(
        Messages::MAXIMUM_TELEMETRY_PERIOD_MSEC);
    const std::vector<uint8_t> payload = getPayload(setTelemetryPeriodRequest);

    // check the payload
    CAPTURE(payload);
    REQUIRE(payload == std::vector<uint8_t>({0xFF, 0xFF}));

    // check the set telemetry period request of a received frame
    const Messages::SetTelemetryPeriodRequest receivedSetTelemetryPeriodRequest(getFrame(payload));
    REQUIRE(receivedSetTelemetryPeriodRequest.getLikelyValid());
    REQUIRE(receivedSetTelemetryPeriodRequest.getTelemetryPeriod() == 65535ms);
}

TEST_CASE_METHOD(TestMessages,
                 "Set telemetry period request outside its field stays empty",
                 "[Messages][testSetTelemetryPeriodRequestOutOfRange]")
{
    // create set telemetry period requests outside the field
    const Messages::SetTelemetryPeriodRequest longSetTelemetryPeriodRequest(65536ms);
    const Messages::SetTelemetryPeriodRequest negativeSetTelemetryPeriodRequest(-1ms);

    // check the empty set telemetry period requests
    REQUIRE(longSetTelemetryPeriodRequest.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(longSetTelemetryPeriodRequest.getLikelyValid());
    REQUIRE(negativeSetTelemetryPeriodRequest.getPayloadLength() == static_cast<size_t>(0));
    REQUIRE_FALSE(negativeSetTelemetryPeriodRequest.getLikelyValid());
}

TEST_CASE_METHOD(TestMessages,
                 "Telemetry report response keeps a negative temperature",
                 "[Messages][testTelemetryReportResponse]")
{
    // create a telemetry report response
    const Messages::TelemetryReportResponse telemetryReportResponse(65535, 10ms, 20, -1234);
    const std::vector<uint8_t> payload = getPayload(telemetryReportResponse);

    // check the telemetry report response of a received frame
    const Messages::TelemetryReportResponse receivedTelemetryReportResponse(getFrame(payload));
    REQUIRE(receivedTelemetryReportResponse.getLikelyValid());
    REQUIRE(receivedTelemetryReportResponse.getSequence() == 65535);
    REQUIRE(receivedTelemetryReportResponse.getUptime() == 10ms);
    REQUIRE(receivedTelemetryReportResponse.getFreeHeapSize() == static_cast<uint32_t>(20));
    REQUIRE(receivedTelemetryReportResponse.getTemperature() == -1234);
}

TEST_CASE_METHOD(TestMessages,
                 "Truncated payloads are not likely valid",
                 "[Messages][testTruncatedPayload]")
{
    // create a telemetry report response
    const Messages::TelemetryReportResponse telemetryReportResponse(1, 2ms, 3, 4);
    const std::vector<uint8_t> payload = getPayload(telemetryReportResponse);

    // check every truncated payload
    for (size_t payloadLength = 0; payloadLength < payload.size(); ++payloadLength) {
        CAPTURE(payloadLength);
        const Messages::TelemetryReportResponse receivedTelemetryReportResponse(
            Frame::Frame(0, 0, 0, payload.data(), payloadLength));
        REQUIRE_FALSE(receivedTelemetryReportResponse.getLikelyValid());
        REQUIRE(receivedTelemetryReportResponse.getSequence() == 0);
        REQUIRE(receivedTelemetryReportResponse.getUptime() == 0ms);
        REQUIRE(receivedTelemetryReportResponse.getTemperature() == 0);
    }
}

TEST_CASE_METHOD(TestMessages,
                 "Trailing bytes from a newer peer are tolerated",
                 "[Messages][testTrailingBytes]")
{
    // append a byte to a set telemetry period request
    const Messages::SetTelemetryPeriodRequest setTelemetryPeriodRequest(500ms);
    std::vector<uint8_t> payload = getPayload(setTelemetryPeriodRequest);
    payload.push_back(0x99);

    // check the set telemetry period request of a received frame
    const Messages::SetTelemetryPeriodRequest receivedSetTelemetryPeriodRequest(getFrame(payload));
    REQUIRE(receivedSetTelemetryPeriodRequest.getLikelyValid());
    REQUIRE(receivedSetTelemetryPeriodRequest.getTelemetryPeriod() == 500ms);
}

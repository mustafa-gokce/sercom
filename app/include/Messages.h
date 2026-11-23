#ifndef SERCOM_MESSAGES_H
#define SERCOM_MESSAGES_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "../../core/include/Definitions.h"
#include "../../core/include/Frame.h"
#include "../../core/include/Settings.h"

namespace sercom::Messages {

constexpr size_t PING_REQUEST_SIZE = 4;                 // ping request size in bytes
constexpr size_t PING_RESPONSE_SIZE = 8;                // ping response size in bytes
constexpr size_t DEVICE_INFO_RESPONSE_SIZE = 11;        // fixed device info response size in bytes
constexpr size_t LOG_REPORT_RESPONSE_SIZE = 2;          // fixed log report response size in bytes
constexpr size_t SET_LED_REQUEST_SIZE = 1;              // set LED request size in bytes
constexpr size_t SET_LED_RESPONSE_SIZE = 1;             // set LED response size in bytes
constexpr size_t SET_TELEMETRY_PERIOD_REQUEST_SIZE = 2; // set telemetry period request size in bytes
constexpr size_t TELEMETRY_REPORT_RESPONSE_SIZE = 12;   // telemetry report response size in bytes

constexpr size_t MAXIMUM_DEVICE_NAME_LENGTH = 31; // maximum device name length in characters
constexpr size_t MAXIMUM_LOG_TEXT_LENGTH = 127;   // maximum log text length in characters

constexpr std::chrono::milliseconds MAXIMUM_TELEMETRY_PERIOD_MSEC = std::chrono::milliseconds(
    std::numeric_limits<uint16_t>::max()); // longest telemetry period of the field in milliseconds
constexpr int16_t UNAVAILABLE_TEMPERATURE
    = std::numeric_limits<int16_t>::min(); // temperature without a reading in 0.01 degree steps

/*
 * Message type.
 */
enum class MessageType : uint16_t {
    UNKNOWN = 0x0000, // unknown message type

    // system messages
    PING_REQUEST = 0x0001,         // host asks for a ping response
    PING_RESPONSE = 0x0002,        // device answers a ping request
    DEVICE_INFO_REQUEST = 0x0003,  // host asks for the device information
    DEVICE_INFO_RESPONSE = 0x0004, // device answers with its information
    LOG_REPORT_RESPONSE = 0x0005,  // device reports a log line
    ECHO_REQUEST = 0x0006,         // host sends bytes to echo
    ECHO_RESPONSE = 0x0007,        // device sends the bytes back

    // application messages
    SET_LED_REQUEST = 0x0100,              // host switches the LED
    SET_LED_RESPONSE = 0x0101,             // device reports the LED state
    SET_TELEMETRY_PERIOD_REQUEST = 0x0102, // host changes the telemetry period
    TELEMETRY_REPORT_RESPONSE = 0x0103,    // device reports its telemetry
};

/*
 * Message type to integer.
 */
uint16_t fromMessageType(MessageType messageType);

/*
 * Integer to message type.
 */
MessageType toMessageType(uint16_t messageType);

/*
 * Log level type.
 */
enum class LogLevelType : uint8_t {
    UNKNOWN = 0, // unknown log level
    DEBUG = 1,   // information about the device
    WARNING = 2, // problem of the device
};

/*
 * Log level type to integer.
 */
uint8_t fromLogLevelType(LogLevelType logLevelType);

/*
 * Integer to log level type.
 */
LogLevelType toLogLevelType(uint8_t logLevelType);

/*
 * Get message type as text.
 */
[[nodiscard]] std::string_view getMessageTypeText(MessageType messageType);

/*
 * Get log level type as text.
 */
[[nodiscard]] std::string_view getLogLevelTypeText(LogLevelType logLevelType);

class GenericMessage
{
public:
    /*
     * Message of the demo device protocol that keeps its payload bytes.
     * A message built from more bytes than a payload holds stays empty.
     * A message built from a routed frame keeps the address of its source.
     */
    GenericMessage();
    explicit GenericMessage(const uint8_t *data, size_t size);
    explicit GenericMessage(const Frame::Frame &frame);
    ~GenericMessage() = default;

    // default copy constructor and copy assignment operator
    GenericMessage(const GenericMessage &) = default;
    GenericMessage &operator=(const GenericMessage &) = default;

    // default move constructor and move assignment operator
    GenericMessage(GenericMessage &&) = default;
    GenericMessage &operator=(GenericMessage &&) = default;

public:
    /*
     * Get the first payload byte.
     */
    [[nodiscard]] const uint8_t *getPayload() const;

    /*
     * Get the payload length in bytes.
     */
    [[nodiscard]] size_t getPayloadLength() const;

    /*
     * Get the address of the node that sent the message, unassigned for a direct frame.
     */
    [[nodiscard]] uint8_t getSourceAddress() const;

protected:
    std::array<uint8_t, Settings::MaximumPayloadSize> payload; // payload as byte array
    size_t payloadLength;                                      // payload length in bytes
    uint8_t sourceAddress;                                     // node that sent the message
};

class PingRequest final : public GenericMessage
{
public:
    /*
     * Ping request that carries a time of the host, the ping response returns it unchanged.
     */
    explicit PingRequest(std::chrono::milliseconds requestTimestamp);
    explicit PingRequest(const Frame::Frame &frame);
    ~PingRequest() = default;

    // default copy constructor and copy assignment operator
    PingRequest(const PingRequest &) = default;
    PingRequest &operator=(const PingRequest &) = default;

    // default move constructor and move assignment operator
    PingRequest(PingRequest &&) = default;
    PingRequest &operator=(PingRequest &&) = default;

public:
    /*
     * Get the request timestamp in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getRequestTimestamp() const;

    /*
     * Get whether the payload holds every field.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class PingResponse final : public GenericMessage
{
public:
    /*
     * Ping response that returns the request timestamp together with the uptime of the device.
     */
    explicit PingResponse(std::chrono::milliseconds requestTimestamp,
                          std::chrono::milliseconds uptime);
    explicit PingResponse(const Frame::Frame &frame);
    ~PingResponse() = default;

    // default copy constructor and copy assignment operator
    PingResponse(const PingResponse &) = default;
    PingResponse &operator=(const PingResponse &) = default;

    // default move constructor and move assignment operator
    PingResponse(PingResponse &&) = default;
    PingResponse &operator=(PingResponse &&) = default;

public:
    /*
     * Get the request timestamp in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getRequestTimestamp() const;

    /*
     * Get the uptime of the device in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getUptime() const;

    /*
     * Get whether the payload holds every field.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class DeviceInfoRequest final : public GenericMessage
{
public:
    /*
     * Device info request without fields.
     */
    DeviceInfoRequest() = default;
    ~DeviceInfoRequest() = default;

    // default copy constructor and copy assignment operator
    DeviceInfoRequest(const DeviceInfoRequest &) = default;
    DeviceInfoRequest &operator=(const DeviceInfoRequest &) = default;

    // default move constructor and move assignment operator
    DeviceInfoRequest(DeviceInfoRequest &&) = default;
    DeviceInfoRequest &operator=(DeviceInfoRequest &&) = default;
};

class DeviceInfoResponse final : public GenericMessage
{
public:
    /*
     * Device info response with the firmware version, the uptime, the free heap and the name.
     * A device name above the maximum device name length leaves the response empty.
     * A device name that does not fit into the payload leaves the response empty.
     */
    explicit DeviceInfoResponse(uint8_t firmwareVersionMajor,
                                uint8_t firmwareVersionMinor,
                                std::chrono::milliseconds uptime,
                                uint32_t freeHeapSize,
                                std::string_view deviceName);
    explicit DeviceInfoResponse(const Frame::Frame &frame);
    ~DeviceInfoResponse() = default;

    // default copy constructor and copy assignment operator
    DeviceInfoResponse(const DeviceInfoResponse &) = default;
    DeviceInfoResponse &operator=(const DeviceInfoResponse &) = default;

    // default move constructor and move assignment operator
    DeviceInfoResponse(DeviceInfoResponse &&) = default;
    DeviceInfoResponse &operator=(DeviceInfoResponse &&) = default;

public:
    /*
     * Get the major firmware version.
     */
    [[nodiscard]] uint8_t getFirmwareVersionMajor() const;

    /*
     * Get the minor firmware version.
     */
    [[nodiscard]] uint8_t getFirmwareVersionMinor() const;

    /*
     * Get the uptime of the device in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getUptime() const;

    /*
     * Get the free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const;

    /*
     * Get the device name, valid as long as the response exists.
     */
    [[nodiscard]] std::string_view getDeviceName() const;

    /*
     * Get whether the payload holds every field and the whole device name.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class LogReportResponse final : public GenericMessage
{
public:
    /*
     * Log report response that the device sends on its own.
     * A text above the maximum log text length leaves the response empty.
     * A text that does not fit into the payload leaves the response empty.
     */
    explicit LogReportResponse(LogLevelType logLevelType, std::string_view text);
    explicit LogReportResponse(const Frame::Frame &frame);
    ~LogReportResponse() = default;

    // default copy constructor and copy assignment operator
    LogReportResponse(const LogReportResponse &) = default;
    LogReportResponse &operator=(const LogReportResponse &) = default;

    // default move constructor and move assignment operator
    LogReportResponse(LogReportResponse &&) = default;
    LogReportResponse &operator=(LogReportResponse &&) = default;

public:
    /*
     * Get the log level type.
     */
    [[nodiscard]] LogLevelType getLogLevelType() const;

    /*
     * Get the text, valid as long as the response exists.
     */
    [[nodiscard]] std::string_view getText() const;

    /*
     * Get whether the payload holds every field and the whole text.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class EchoRequest final : public GenericMessage
{
public:
    /*
     * Echo request whose payload bytes the echo response returns unchanged.
     * Data that does not fit into the payload leaves the request empty.
     */
    explicit EchoRequest(const uint8_t *data, size_t size);
    explicit EchoRequest(const Frame::Frame &frame);
    ~EchoRequest() = default;

    // default copy constructor and copy assignment operator
    EchoRequest(const EchoRequest &) = default;
    EchoRequest &operator=(const EchoRequest &) = default;

    // default move constructor and move assignment operator
    EchoRequest(EchoRequest &&) = default;
    EchoRequest &operator=(EchoRequest &&) = default;
};

class EchoResponse final : public GenericMessage
{
public:
    /*
     * Echo response with the payload bytes of its echo request.
     * Data that does not fit into the payload leaves the response empty.
     */
    explicit EchoResponse(const uint8_t *data, size_t size);
    explicit EchoResponse(const Frame::Frame &frame);
    ~EchoResponse() = default;

    // default copy constructor and copy assignment operator
    EchoResponse(const EchoResponse &) = default;
    EchoResponse &operator=(const EchoResponse &) = default;

    // default move constructor and move assignment operator
    EchoResponse(EchoResponse &&) = default;
    EchoResponse &operator=(EchoResponse &&) = default;
};

class SetLedRequest final : public GenericMessage
{
public:
    /*
     * Set LED request that switches the LED of the device.
     */
    explicit SetLedRequest(bool ledOn);
    explicit SetLedRequest(const Frame::Frame &frame);
    ~SetLedRequest() = default;

    // default copy constructor and copy assignment operator
    SetLedRequest(const SetLedRequest &) = default;
    SetLedRequest &operator=(const SetLedRequest &) = default;

    // default move constructor and move assignment operator
    SetLedRequest(SetLedRequest &&) = default;
    SetLedRequest &operator=(SetLedRequest &&) = default;

public:
    /*
     * Get whether the LED is switched on.
     */
    [[nodiscard]] bool getLedOn() const;

    /*
     * Get whether the payload holds the LED state as zero or one.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class SetLedResponse final : public GenericMessage
{
public:
    /*
     * Set LED response with the LED state that the hardware reports.
     */
    explicit SetLedResponse(bool ledOn);
    explicit SetLedResponse(const Frame::Frame &frame);
    ~SetLedResponse() = default;

    // default copy constructor and copy assignment operator
    SetLedResponse(const SetLedResponse &) = default;
    SetLedResponse &operator=(const SetLedResponse &) = default;

    // default move constructor and move assignment operator
    SetLedResponse(SetLedResponse &&) = default;
    SetLedResponse &operator=(SetLedResponse &&) = default;

public:
    /*
     * Get whether the LED is on.
     */
    [[nodiscard]] bool getLedOn() const;

    /*
     * Get whether the payload holds the LED state as zero or one.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class SetTelemetryPeriodRequest final : public GenericMessage
{
public:
    /*
     * Set telemetry period request, a zero telemetry period stops the telemetry.
     * A telemetry period outside the field leaves the request empty.
     */
    explicit SetTelemetryPeriodRequest(std::chrono::milliseconds telemetryPeriod);
    explicit SetTelemetryPeriodRequest(const Frame::Frame &frame);
    ~SetTelemetryPeriodRequest() = default;

    // default copy constructor and copy assignment operator
    SetTelemetryPeriodRequest(const SetTelemetryPeriodRequest &) = default;
    SetTelemetryPeriodRequest &operator=(const SetTelemetryPeriodRequest &) = default;

    // default move constructor and move assignment operator
    SetTelemetryPeriodRequest(SetTelemetryPeriodRequest &&) = default;
    SetTelemetryPeriodRequest &operator=(SetTelemetryPeriodRequest &&) = default;

public:
    /*
     * Get the telemetry period in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTelemetryPeriod() const;

    /*
     * Get whether the payload holds every field.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

class TelemetryReportResponse final : public GenericMessage
{
public:
    /*
     * Telemetry report response that the device sends once per telemetry period.
     */
    explicit TelemetryReportResponse(uint16_t sequence,
                                     std::chrono::milliseconds uptime,
                                     uint32_t freeHeapSize,
                                     int16_t temperature);
    explicit TelemetryReportResponse(const Frame::Frame &frame);
    ~TelemetryReportResponse() = default;

    // default copy constructor and copy assignment operator
    TelemetryReportResponse(const TelemetryReportResponse &) = default;
    TelemetryReportResponse &operator=(const TelemetryReportResponse &) = default;

    // default move constructor and move assignment operator
    TelemetryReportResponse(TelemetryReportResponse &&) = default;
    TelemetryReportResponse &operator=(TelemetryReportResponse &&) = default;

public:
    /*
     * Get the sequence that wraps from 65535 to 0.
     */
    [[nodiscard]] uint16_t getSequence() const;

    /*
     * Get the uptime of the device in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getUptime() const;

    /*
     * Get the free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const;

    /*
     * Get the temperature in 0.01 degree Celsius steps.
     */
    [[nodiscard]] int16_t getTemperature() const;

    /*
     * Get whether the payload holds every field.
     */
    [[nodiscard]] bool getLikelyValid() const;
};

} // namespace sercom::Messages

// check definitions
static_assert(sercom::Messages::TELEMETRY_REPORT_RESPONSE_SIZE
                  <= sercom::Settings::MaximumPayloadSize,
              "Telemetry report response must fit into a payload");

#endif // SERCOM_MESSAGES_H

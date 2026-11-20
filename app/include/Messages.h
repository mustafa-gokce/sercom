#ifndef SERCOM_MESSAGES_H
#define SERCOM_MESSAGES_H

#include <cstdint>
#include <string_view>

namespace sercom::Messages {

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

} // namespace sercom::Messages

#endif // SERCOM_MESSAGES_H

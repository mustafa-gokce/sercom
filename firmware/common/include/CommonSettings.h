#ifndef SERCOM_COMMON_SETTINGS_H
#define SERCOM_COMMON_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "../../../app/include/Messages.h"
#include "../../../core/include/Settings.h"

namespace sercom::CommonSettings {

using namespace std::chrono_literals;

constexpr uint32_t SERIAL_BAUD_RATE = 115200;  // baud rate of the protocol serial port
constexpr uint32_t CONSOLE_BAUD_RATE = 115200; // baud rate of the console serial port

constexpr size_t CONSOLE_LINE_SIZE = 400;           // console line size in characters
constexpr size_t CONSOLE_FIELD_SIZE = 64;           // console field size in characters
constexpr size_t CONSOLE_MAXIMUM_ESCAPE_LENGTH = 2; // longest escape of one text byte in characters
constexpr size_t CONSOLE_HEX_BYTE_LENGTH = 3;       // characters of one byte in hex with its space

constexpr size_t PROTOCOL_CHUNK_SIZE = 128; // protocol chunk size in bytes
constexpr size_t LOG_LINE_SIZE = 96;        // log line size in characters

constexpr std::chrono::milliseconds HEARTBEAT_PERIOD_MSEC
    = 5 * 1000ms; // heartbeat period in milliseconds
constexpr std::chrono::milliseconds ACTIVITY_BLINK_DURATION_MSEC
    = 30ms; // time the LED shows an activity blink in milliseconds
constexpr std::chrono::milliseconds ACTIVITY_BLINK_PERIOD_MSEC
    = 100ms; // shortest time from one activity blink to the next in milliseconds

} // namespace sercom::CommonSettings

// check settings
static_assert(sercom::CommonSettings::LOG_LINE_SIZE <= sercom::Messages::MAXIMUM_LOG_TEXT_LENGTH,
              "Log line size must be at most 127 characters");
static_assert(sercom::CommonSettings::CONSOLE_LINE_SIZE
                  > sercom::CommonSettings::CONSOLE_MAXIMUM_ESCAPE_LENGTH
                            * sercom::Settings::MAXIMUM_UNFRAMED_DATA_SIZE
                        + sercom::CommonSettings::CONSOLE_FIELD_SIZE,
              "Console line size must hold the escaped unframed text and the line prefix");
static_assert(sercom::CommonSettings::CONSOLE_LINE_SIZE
                  > sercom::CommonSettings::CONSOLE_HEX_BYTE_LENGTH
                            * sercom::Settings::MAXIMUM_UNFRAMED_DATA_SIZE
                        + sercom::CommonSettings::CONSOLE_FIELD_SIZE,
              "Console line size must hold the unframed bytes in hex and the line prefix");
static_assert(sercom::CommonSettings::CONSOLE_FIELD_SIZE
                  > sercom::CommonSettings::CONSOLE_HEX_BYTE_LENGTH
                        * sercom::Settings::MAXIMUM_TRACED_PAYLOAD_SIZE,
              "Console field size must hold the traced payload in hex");

#endif // SERCOM_COMMON_SETTINGS_H

#ifndef SERCOM_HOST_SETTINGS_H
#define SERCOM_HOST_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sercom::HostSettings {

using namespace std::chrono_literals;

constexpr int INVALID_COMMAND_LINE_EXIT_CODE = 64; // exit code of an invalid command line

constexpr uint32_t DEFAULT_BAUD_RATE = 115200; // default baud rate in bits per second
constexpr size_t READ_BUFFER_SIZE = 1 * 1024;  // serial port read buffer size in bytes

constexpr std::chrono::milliseconds WRITE_TIMEOUT_MSEC
    = 1 * 1000ms; // serial port write timeout in milliseconds

constexpr std::string_view DEFAULT_DEVICE_NAME = "sercom-device-sim"; // default device name

} // namespace sercom::HostSettings

#endif // SERCOM_HOST_SETTINGS_H

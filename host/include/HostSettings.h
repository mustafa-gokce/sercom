#ifndef SERCOM_HOST_SETTINGS_H
#define SERCOM_HOST_SETTINGS_H

#include <cstddef>
#include <cstdint>

namespace sercom::HostSettings {

constexpr int INVALID_COMMAND_LINE_EXIT_CODE = 64; // exit code of an invalid command line

constexpr uint32_t DEFAULT_BAUD_RATE = 115200; // default baud rate in bits per second
constexpr size_t READ_BUFFER_SIZE = 1 * 1024;  // serial port read buffer size in bytes

} // namespace sercom::HostSettings

#endif // SERCOM_HOST_SETTINGS_H

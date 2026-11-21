#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <chrono>
#include <cstdint>

namespace sercom::FirmwareSettings {

using namespace std::chrono_literals;

constexpr uint32_t SERIAL_BAUD_RATE = 115200; // baud rate of the protocol serial port

constexpr std::chrono::milliseconds PING_REQUEST_PERIOD_MSEC
    = 1 * 1000ms; // ping request frame period in milliseconds

} // namespace sercom::FirmwareSettings

#endif // SERCOM_FIRMWARE_SETTINGS_H

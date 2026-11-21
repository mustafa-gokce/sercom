#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareSettings {

constexpr uint32_t SERIAL_BAUD_RATE = 115200;            // baud rate of the protocol serial port
constexpr size_t SERIAL_RECEIVE_BUFFER_SIZE = 1 * 1024;  // driver receive buffer size in bytes
constexpr size_t SERIAL_TRANSMIT_BUFFER_SIZE = 1 * 1024; // driver transmit buffer size in bytes

constexpr uint8_t LED_PIN = 2;                           // GPIO of the on board LED
constexpr std::string_view DEVICE_NAME = "sercom-esp32"; // reported device name

constexpr size_t RECEIVE_CHUNK_SIZE = 64; // receive chunk size in bytes

} // namespace sercom::FirmwareSettings

// check settings
static_assert(sercom::FirmwareSettings::DEVICE_NAME.size()
                  <= sercom::Messages::MAXIMUM_DEVICE_NAME_LENGTH,
              "Device name must be at most 31 characters");

#endif // SERCOM_FIRMWARE_SETTINGS_H

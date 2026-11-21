#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareSettings {

using namespace std::chrono_literals;

constexpr uint32_t SERIAL_BAUD_RATE = 115200;            // baud rate of the protocol serial port
constexpr size_t SERIAL_RECEIVE_BUFFER_SIZE = 1 * 1024;  // driver receive buffer size in bytes
constexpr size_t SERIAL_TRANSMIT_BUFFER_SIZE = 1 * 1024; // driver transmit buffer size in bytes

constexpr uint8_t LED_PIN = 2;                           // GPIO of the on board LED
constexpr std::string_view DEVICE_NAME = "sercom-esp32"; // reported device name

constexpr uint32_t UART_RECEIVE_TASK_STACK_SIZE = 3 * 1024; // receive task stack size in bytes
constexpr uint32_t PROTOCOL_TASK_STACK_SIZE = 8 * 1024;     // protocol task stack size in bytes
constexpr uint32_t UART_RECEIVE_TASK_PRIORITY = 3;          // receive task priority
constexpr uint32_t PROTOCOL_TASK_PRIORITY = 2;              // protocol task priority
constexpr int APPLICATION_CORE = 1;                         // processor core of the tasks

constexpr size_t RECEIVE_STREAM_BUFFER_SIZE = 2 * 1024; // receive stream buffer size in bytes
constexpr size_t RECEIVE_STREAM_TRIGGER_SIZE = 1;       // receive stream trigger size in bytes
constexpr size_t RECEIVE_CHUNK_SIZE = 64;               // receive chunk size in bytes
constexpr size_t PROTOCOL_CHUNK_SIZE = 128;             // protocol chunk size in bytes

constexpr std::chrono::milliseconds UART_IDLE_DELAY_MSEC = 1ms; // idle receive delay in milliseconds
constexpr std::chrono::milliseconds PROTOCOL_TICK_PERIOD_MSEC
    = 5ms; // protocol tick period in milliseconds

} // namespace sercom::FirmwareSettings

// check settings
static_assert(sercom::FirmwareSettings::DEVICE_NAME.size()
                  <= sercom::Messages::MAXIMUM_DEVICE_NAME_LENGTH,
              "Device name must be at most 31 characters");

#endif // SERCOM_FIRMWARE_SETTINGS_H

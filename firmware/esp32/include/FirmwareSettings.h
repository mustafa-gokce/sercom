#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareSettings {

using namespace std::chrono_literals;

extern const std::string_view SerialPortName; // name of the protocol serial port

constexpr std::string_view UART0_PORT_NAME = "UART0"; // name of the serial port on the USB bridge

constexpr uint32_t SERIAL_BAUD_RATE = 115200;            // baud rate of the protocol serial port
constexpr size_t SERIAL_RECEIVE_BUFFER_SIZE = 1 * 1024;  // driver receive buffer size in bytes
constexpr size_t SERIAL_TRANSMIT_BUFFER_SIZE = 1 * 1024; // driver transmit buffer size in bytes

constexpr uint8_t LED_PIN = 2;                           // GPIO of the on board LED
constexpr std::string_view DEVICE_NAME = "sercom-esp32"; // reported device name

constexpr uint32_t UART_RECEIVE_TASK_STACK_SIZE = 3 * 1024; // receive task stack size in bytes
constexpr uint32_t PROTOCOL_TASK_STACK_SIZE = 8 * 1024;     // protocol task stack size in bytes
constexpr uint32_t HEARTBEAT_TASK_STACK_SIZE = 3 * 1024;    // heartbeat task stack size in bytes
constexpr uint32_t UART_RECEIVE_TASK_PRIORITY = 3;          // receive task priority
constexpr uint32_t PROTOCOL_TASK_PRIORITY = 2;              // protocol task priority
constexpr uint32_t HEARTBEAT_TASK_PRIORITY = 1;             // heartbeat task priority
constexpr int APPLICATION_CORE = 1;                         // processor core of the tasks

constexpr size_t RECEIVE_STREAM_BUFFER_SIZE = 2 * 1024; // receive stream buffer size in bytes
constexpr size_t RECEIVE_STREAM_TRIGGER_SIZE = 1;       // receive stream trigger size in bytes
constexpr size_t RECEIVE_CHUNK_SIZE = 64;               // receive chunk size in bytes
constexpr size_t PROTOCOL_CHUNK_SIZE = 128;             // protocol chunk size in bytes
constexpr uint32_t LOG_QUEUE_LENGTH = 8;                // log queue length in log lines
constexpr size_t LOG_LINE_SIZE = 96;                    // log line size in characters

constexpr std::chrono::milliseconds UART_IDLE_DELAY_MSEC = 1ms; // idle receive delay in milliseconds
constexpr std::chrono::milliseconds PROTOCOL_TICK_PERIOD_MSEC
    = 5ms; // protocol tick period in milliseconds
constexpr std::chrono::milliseconds HEARTBEAT_PERIOD_MSEC
    = 5 * 1000ms; // heartbeat period in milliseconds
constexpr std::chrono::milliseconds LOG_QUEUE_TIMEOUT_MSEC = 0ms; // log queue timeout in milliseconds

} // namespace sercom::FirmwareSettings

// check settings
static_assert(sercom::FirmwareSettings::DEVICE_NAME.size()
                  <= sercom::Messages::MAXIMUM_DEVICE_NAME_LENGTH,
              "Device name must be at most 31 characters");
static_assert(sercom::FirmwareSettings::LOG_LINE_SIZE <= sercom::Messages::MAXIMUM_LOG_TEXT_LENGTH,
              "Log line size must be at most 127 characters");

#endif // SERCOM_FIRMWARE_SETTINGS_H

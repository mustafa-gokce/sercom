#ifndef SERCOM_FIRMWARE_SETTINGS_H
#define SERCOM_FIRMWARE_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../app/include/Messages.h"

namespace sercom::FirmwareSettings {

using namespace std::chrono_literals;

extern const bool ConsolePrinted;              // console on UART0 and protocol on UART2
extern const bool ControllerRun;               // controller role instead of the device role
extern const std::string_view ApplicationName; // name of the application on the protocol serial port
extern const std::string_view SerialPortName;  // name of the protocol serial port
extern const int8_t SerialReceivePin;          // GPIO of the protocol receive pin
extern const int8_t SerialTransmitPin;         // GPIO of the protocol transmit pin

extern const bool RouterRun;                     // device role as a router between two serial ports
extern const uint8_t NodeAddress;                // address of the node, unassigned without one
extern const uint8_t ProxyAddress;               // address of the neighbour below without one
extern const std::string_view SecondaryPortName; // name of the secondary serial port of a router
extern const int8_t SecondaryReceivePin;         // GPIO of the secondary receive pin
extern const int8_t SecondaryTransmitPin;        // GPIO of the secondary transmit pin

constexpr std::string_view UART0_PORT_NAME = "UART0"; // name of the serial port on the USB bridge
constexpr int8_t UART0_RECEIVE_PIN = 3;               // GPIO of the UART0 receive pin
constexpr int8_t UART0_TRANSMIT_PIN = 1;              // GPIO of the UART0 transmit pin
constexpr std::string_view UART2_PORT_NAME = "UART2"; // name of the second serial port
constexpr int8_t UART2_RECEIVE_PIN = 16;              // GPIO of the UART2 receive pin
constexpr int8_t UART2_TRANSMIT_PIN = 17;             // GPIO of the UART2 transmit pin

constexpr size_t SERIAL_RECEIVE_BUFFER_SIZE = 1 * 1024;  // driver receive buffer size in bytes
constexpr size_t SERIAL_TRANSMIT_BUFFER_SIZE = 1 * 1024; // driver transmit buffer size in bytes

constexpr size_t CONSOLE_TRANSMIT_BUFFER_SIZE = 4 * 1024; // console transmit buffer size in bytes
constexpr size_t CONSOLE_STREAM_TRIGGER_SIZE = 1;         // console stream trigger size in bytes
constexpr size_t CONSOLE_CHUNK_SIZE = 128;                // console chunk size in bytes

constexpr uint8_t LED_PIN = 2;                           // GPIO of the on board LED
constexpr std::string_view DEVICE_NAME = "sercom-esp32"; // reported device name

constexpr uint32_t UART_RECEIVE_TASK_STACK_SIZE = 3 * 1024; // receive task stack size in bytes
constexpr uint32_t PROTOCOL_TASK_STACK_SIZE = 8 * 1024;     // protocol task stack size in bytes
constexpr uint32_t HEARTBEAT_TASK_STACK_SIZE = 3 * 1024;    // heartbeat task stack size in bytes
constexpr uint32_t CONSOLE_TASK_STACK_SIZE = 3 * 1024;      // console task stack size in bytes
constexpr uint32_t UART_RECEIVE_TASK_PRIORITY = 3;          // receive task priority
constexpr uint32_t PROTOCOL_TASK_PRIORITY = 2;              // protocol task priority
constexpr uint32_t HEARTBEAT_TASK_PRIORITY = 1;             // heartbeat task priority
constexpr uint32_t CONSOLE_TASK_PRIORITY = 1;               // console task priority
constexpr int APPLICATION_CORE = 1;                         // processor core of the tasks

constexpr size_t RECEIVE_STREAM_BUFFER_SIZE = 2 * 1024; // receive stream buffer size in bytes
constexpr size_t RECEIVE_STREAM_TRIGGER_SIZE = 1;       // receive stream trigger size in bytes
constexpr size_t RECEIVE_CHUNK_SIZE = 64;               // receive chunk size in bytes
constexpr uint32_t LOG_QUEUE_LENGTH = 8;                // log queue length in log lines

constexpr std::chrono::milliseconds UART_IDLE_DELAY_MSEC = 1ms; // idle receive delay in milliseconds
constexpr std::chrono::milliseconds PROTOCOL_TICK_PERIOD_MSEC
    = 5ms; // protocol tick period in milliseconds
constexpr std::chrono::milliseconds LOG_QUEUE_TIMEOUT_MSEC = 0ms; // log queue timeout in milliseconds
constexpr std::chrono::milliseconds CONSOLE_STREAM_TIMEOUT_MSEC
    = 0ms; // console stream timeout in milliseconds

constexpr uint32_t ROUTER_STATISTICS_QUEUE_LENGTH = 1; // router statistics queue length for overwrites
constexpr std::chrono::milliseconds SECONDARY_RECEIVE_TIMEOUT_MSEC
    = 0ms; // secondary receive timeout in milliseconds
constexpr std::chrono::milliseconds ROUTER_STATISTICS_QUEUE_TIMEOUT_MSEC
    = 0ms; // router statistics queue timeout in milliseconds

} // namespace sercom::FirmwareSettings

// check settings
static_assert(sercom::FirmwareSettings::DEVICE_NAME.size()
                  <= sercom::Messages::MAXIMUM_DEVICE_NAME_LENGTH,
              "Device name must be at most 31 characters");

#endif // SERCOM_FIRMWARE_SETTINGS_H

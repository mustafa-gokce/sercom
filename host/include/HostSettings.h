#ifndef SERCOM_HOST_SETTINGS_H
#define SERCOM_HOST_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sercom::HostSettings {

using namespace std::chrono_literals;

constexpr int INVALID_COMMAND_LINE_EXIT_CODE = 64; // exit code of an invalid command line
constexpr int INTERRUPTED_EXIT_CODE = 130;         // exit code after a stop signal

constexpr uint32_t DEFAULT_BAUD_RATE = 115200; // default baud rate in bits per second
constexpr size_t READ_BUFFER_SIZE = 1 * 1024;  // serial port read buffer size in bytes

constexpr std::chrono::milliseconds READ_TIMEOUT_MSEC
    = 2ms; // serial port read timeout in milliseconds
constexpr std::chrono::milliseconds WRITE_TIMEOUT_MSEC
    = 1 * 1000ms; // serial port write timeout in milliseconds
constexpr std::chrono::milliseconds SERIAL_PORT_RECONNECT_PERIOD_MSEC
    = 1 * 1000ms; // serial port reconnect period in milliseconds

constexpr std::chrono::milliseconds DEVICE_PERIODIC_REQUEST_PERIOD_MSEC
    = 1 * 1000ms; // device periodic request period in milliseconds
constexpr std::chrono::milliseconds DEVICE_DEACTIVATION_PERIOD_MSEC
    = DEVICE_PERIODIC_REQUEST_PERIOD_MSEC * 10; // device deactivation period in milliseconds
constexpr std::chrono::milliseconds RESPONSE_TIMEOUT_MSEC
    = 3 * 1000ms; // response timeout in milliseconds
constexpr std::chrono::milliseconds PING_PERIOD_MSEC
    = 200ms; // ping command request period in milliseconds

constexpr std::chrono::milliseconds PERIOD_ARGUMENT_UNIT_MSEC
    = 1ms; // unit of a period argument in milliseconds
constexpr std::chrono::milliseconds DURATION_ARGUMENT_UNIT_MSEC
    = 1 * 1000ms; // unit of a duration argument in milliseconds
constexpr std::chrono::milliseconds DEFAULT_COMMAND_DURATION_MSEC
    = 5 * 1000ms; // default command duration in milliseconds
constexpr std::chrono::milliseconds MAXIMUM_COMMAND_DURATION_MSEC
    = 24 * 60 * 60 * 1000ms;                    // maximum command duration in milliseconds
constexpr uint32_t MAXIMUM_PING_COUNT = 100000; // maximum ping requests of the ping command

constexpr std::string_view DEFAULT_DEVICE_NAME = "sercom-device-sim"; // default device name
constexpr uint32_t SIMULATED_FREE_HEAP_SIZE = 180000; // simulated free heap size in bytes
constexpr int16_t SIMULATED_TEMPERATURE = 2500; // simulated mean temperature in 0.01 degree steps
constexpr int16_t SIMULATED_TEMPERATURE_AMPLITUDE
    = 300; // simulated temperature amplitude in 0.01 degree steps
constexpr std::chrono::milliseconds SIMULATED_TEMPERATURE_PERIOD_MSEC
    = 60 * 1000ms; // simulated temperature period in milliseconds

constexpr double PI = 3.14159265358979323846; // ratio of circumference to diameter

} // namespace sercom::HostSettings

#endif // SERCOM_HOST_SETTINGS_H

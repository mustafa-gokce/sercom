#ifndef SERCOM_SERIAL_PORT_H
#define SERCOM_SERIAL_PORT_H

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "../../core/include/Transport.h"

namespace sercom::SerialPort {

/*
 * Serial port status type.
 */
enum class SerialPortStatusType : uint8_t {
    UNKNOWN = 0,               // unknown serial port status
    OK = 1,                    // operation succeeded
    TIMED_OUT = 2,             // no byte arrived within the timeout
    NOT_OPEN = 3,              // serial port is not open
    OPEN_FAILED = 4,           // device could not be opened
    CONFIGURATION_FAILED = 5,  // device did not take the configuration
    UNSUPPORTED_BAUD_RATE = 6, // termios has no speed for the baud rate
    READ_FAILED = 7,           // reading from the device failed
    DISCONNECTED = 8,          // device hung up
};

/*
 * Serial port status type to string.
 */
std::string_view fromSerialPortStatusType(SerialPortStatusType serialPortStatusType);

/*
 * String to serial port status type.
 */
SerialPortStatusType toSerialPortStatusType(std::string_view serialPortStatusType);

/*
 * Check whether termios has a speed for the baud rate.
 */
[[nodiscard]] bool isBaudRateSupported(uint32_t baudRate);

class SerialPort final : public Transport::Transport
{
public:
    /*
     * Serial port on Linux in raw 8N1 mode without flow control.
     */
    SerialPort();
    ~SerialPort() final;

    // delete copy constructor and copy assignment operator
    SerialPort(const SerialPort &) = delete;
    SerialPort &operator=(const SerialPort &) = delete;

    // delete move constructor and move assignment operator
    SerialPort(SerialPort &&) = delete;
    SerialPort &operator=(SerialPort &&) = delete;

public:
    /*
     * Open and configure a serial device or pseudo terminal.
     * Close an open port first.
     */
    [[nodiscard]] SerialPortStatusType open(const std::string &portName, uint32_t baudRate);

    /*
     * Close the port.
     * Closing a closed port does nothing.
     */
    void close();

    /*
     * Wait up to the timeout for bytes and read at most the capacity of what is available.
     */
    [[nodiscard]] SerialPortStatusType read(uint8_t *data,
                                            size_t capacity,
                                            size_t &size,
                                            std::chrono::milliseconds timeout);

    /*
     * Send every byte of the data and wait while the driver buffer is full.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, size_t size) final;

public:
    /*
     * Get whether the port is open.
     */
    [[nodiscard]] bool getOpened() const;

    /*
     * Get the name of the open port.
     */
    [[nodiscard]] std::string getPortName() const;

    /*
     * Get the native file descriptor or minus one when the port is closed.
     */
    [[nodiscard]] int getFileDescriptor() const;

private:
    int fileDescriptor;   // native handle or minus one when closed
    std::string portName; // device path
};

} // namespace sercom::SerialPort

#endif // SERCOM_SERIAL_PORT_H

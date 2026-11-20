#ifndef SERCOM_SERIAL_PORT_H
#define SERCOM_SERIAL_PORT_H

#include <cstdint>
#include <string>

namespace sercom::SerialPort {

class SerialPort final
{
public:
    /*
     * Serial port on Linux in raw 8N1 mode without flow control.
     */
    SerialPort();
    ~SerialPort();

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
    [[nodiscard]] bool open(const std::string &portName, uint32_t baudRate);

    /*
     * Close the port.
     * Closing a closed port does nothing.
     */
    void close();

public:
    /*
     * Get whether the port is open.
     */
    [[nodiscard]] bool getOpened() const;

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

#include "../include/SerialPort.h"

#include <chrono>

#if defined(__linux__)
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace sercom::SerialPort {

using namespace std::chrono_literals;

namespace {

/*
 * Convert a baud rate to its termios speed or to B0 when termios does not know it.
 */
speed_t toTermiosSpeed(const uint32_t baudRate)
{
    switch (baudRate) {
    case 9600: {
        return B9600;
    }
    case 19200: {
        return B19200;
    }
    case 38400: {
        return B38400;
    }
    case 57600: {
        return B57600;
    }
    case 115200: {
        return B115200;
    }
    case 230400: {
        return B230400;
    }
    case 460800: {
        return B460800;
    }
    case 921600: {
        return B921600;
    }
    default: {
        return B0;
    }
    }
}

} // namespace

SerialPort::SerialPort()
    : fileDescriptor(-1)
{}

SerialPort::~SerialPort()
{
    // close the device
    this->close();
}

bool SerialPort::open(const std::string &portName, const uint32_t baudRate)
{
    // close the open port
    this->close();

    // baud rate must be known to termios
    const speed_t speed = toTermiosSpeed(baudRate);
    if (speed == B0) {
        return false;
    }

    // device must open without becoming the controlling terminal
    const int newFileDescriptor = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (newFileDescriptor < 0) {
        return false;
    }

    // current settings of the device must be readable
    termios tty = termios();
    if (::tcgetattr(newFileDescriptor, &tty) != 0) {
        ::close(newFileDescriptor);
        return false;
    }

    // use raw mode with 8 data bits and no parity
    ::cfmakeraw(&tty);

    // use one stop bit without flow control
    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);

    // enable the receiver and ignore the modem lines
    tty.c_cflag |= CREAD | CLOCAL;

    // NOTE: termios counts the read timeout in tenths of a second
    static constexpr std::chrono::milliseconds readTimeout = 1 * 1000ms;
    static constexpr std::chrono::milliseconds readTimeoutUnit = 100ms;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = static_cast<cc_t>(readTimeout / readTimeoutUnit);

    // baud rate must apply in both directions
    if (::cfsetispeed(&tty, speed) != 0 || ::cfsetospeed(&tty, speed) != 0) {
        ::close(newFileDescriptor);
        return false;
    }

    // settings must apply immediately
    if (::tcsetattr(newFileDescriptor, TCSANOW, &tty) != 0) {
        ::close(newFileDescriptor);
        return false;
    }

    // NOTE: toggling DTR or RTS resets many ESP32 boards
    ::tcflush(newFileDescriptor, TCIOFLUSH);

    // remember the open port
    this->fileDescriptor = newFileDescriptor;
    this->portName = portName;

    // serial port opened successfully
    return true;
}

void SerialPort::close()
{
    // port must be open
    if (this->fileDescriptor < 0) {
        return;
    }

    // close the device
    ::close(this->fileDescriptor);

    // forget the closed port
    this->fileDescriptor = -1;
    this->portName.clear();
}

bool SerialPort::getOpened() const
{
    // expose whether the port is open
    return this->fileDescriptor >= 0;
}

int SerialPort::getFileDescriptor() const
{
    // expose the file descriptor
    return this->fileDescriptor;
}

} // namespace sercom::SerialPort

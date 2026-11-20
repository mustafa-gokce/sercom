#include "../include/SerialPort.h"

#if defined(__linux__)
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "../include/HostSettings.h"

namespace sercom::SerialPort {

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

std::string_view fromSerialPortStatusType(const SerialPortStatusType serialPortStatusType)
{
    switch (serialPortStatusType) {
    case SerialPortStatusType::OK: {
        return {"OK"};
    }
    case SerialPortStatusType::TIMED_OUT: {
        return {"TIMED_OUT"};
    }
    case SerialPortStatusType::NOT_OPEN: {
        return {"NOT_OPEN"};
    }
    case SerialPortStatusType::OPEN_FAILED: {
        return {"OPEN_FAILED"};
    }
    case SerialPortStatusType::CONFIGURATION_FAILED: {
        return {"CONFIGURATION_FAILED"};
    }
    case SerialPortStatusType::UNSUPPORTED_BAUD_RATE: {
        return {"UNSUPPORTED_BAUD_RATE"};
    }
    case SerialPortStatusType::READ_FAILED: {
        return {"READ_FAILED"};
    }
    case SerialPortStatusType::DISCONNECTED: {
        return {"DISCONNECTED"};
    }
    case SerialPortStatusType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

SerialPortStatusType toSerialPortStatusType(const std::string_view serialPortStatusType)
{
    if (serialPortStatusType == std::string_view("OK")) {
        return SerialPortStatusType::OK;
    }
    if (serialPortStatusType == std::string_view("TIMED_OUT")) {
        return SerialPortStatusType::TIMED_OUT;
    }
    if (serialPortStatusType == std::string_view("NOT_OPEN")) {
        return SerialPortStatusType::NOT_OPEN;
    }
    if (serialPortStatusType == std::string_view("OPEN_FAILED")) {
        return SerialPortStatusType::OPEN_FAILED;
    }
    if (serialPortStatusType == std::string_view("CONFIGURATION_FAILED")) {
        return SerialPortStatusType::CONFIGURATION_FAILED;
    }
    if (serialPortStatusType == std::string_view("UNSUPPORTED_BAUD_RATE")) {
        return SerialPortStatusType::UNSUPPORTED_BAUD_RATE;
    }
    if (serialPortStatusType == std::string_view("READ_FAILED")) {
        return SerialPortStatusType::READ_FAILED;
    }
    if (serialPortStatusType == std::string_view("DISCONNECTED")) {
        return SerialPortStatusType::DISCONNECTED;
    }
    return SerialPortStatusType::UNKNOWN;
}

bool isBaudRateSupported(const uint32_t baudRate)
{
    // expose whether termios knows the baud rate
    return toTermiosSpeed(baudRate) != B0;
}

SerialPort::SerialPort()
    : fileDescriptor(-1)
{}

SerialPort::~SerialPort()
{
    // close the device
    this->close();
}

SerialPortStatusType SerialPort::open(const std::string &portName, const uint32_t baudRate)
{
    // close the open port
    this->close();

    // baud rate must be known to termios
    const speed_t speed = toTermiosSpeed(baudRate);
    if (speed == B0) {
        return SerialPortStatusType::UNSUPPORTED_BAUD_RATE;
    }

    // device must open without becoming the controlling terminal
    const int newFileDescriptor = ::open(portName.c_str(),
                                         O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (newFileDescriptor < 0) {
        return SerialPortStatusType::OPEN_FAILED;
    }

    // current settings of the device must be readable
    termios tty = termios();
    if (::tcgetattr(newFileDescriptor, &tty) != 0) {
        ::close(newFileDescriptor);
        return SerialPortStatusType::CONFIGURATION_FAILED;
    }

    // use raw mode with 8 data bits and no parity
    ::cfmakeraw(&tty);

    // use one stop bit without flow control
    tty.c_cflag &= ~static_cast<tcflag_t>(CSTOPB);
    tty.c_cflag &= ~static_cast<tcflag_t>(CRTSCTS);

    // enable the receiver and ignore the modem lines
    tty.c_cflag |= CREAD | CLOCAL;

    // return reads at once and wait in poll
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    // baud rate must apply in both directions
    if (::cfsetispeed(&tty, speed) != 0 || ::cfsetospeed(&tty, speed) != 0) {
        ::close(newFileDescriptor);
        return SerialPortStatusType::CONFIGURATION_FAILED;
    }

    // settings must apply immediately
    if (::tcsetattr(newFileDescriptor, TCSANOW, &tty) != 0) {
        ::close(newFileDescriptor);
        return SerialPortStatusType::CONFIGURATION_FAILED;
    }

    // NOTE: toggling DTR or RTS resets many ESP32 boards
    ::tcflush(newFileDescriptor, TCIOFLUSH);

    // remember the open port
    this->fileDescriptor = newFileDescriptor;
    this->portName = portName;

    // serial port opened successfully
    return SerialPortStatusType::OK;
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

SerialPortStatusType SerialPort::read(uint8_t *data,
                                      const size_t capacity,
                                      size_t &size,
                                      const std::chrono::milliseconds timeout)
{
    // clear the size of the read data
    size = 0;

    // port must be open
    if (this->fileDescriptor < 0) {
        return SerialPortStatusType::NOT_OPEN;
    }

    // bytes must arrive within the timeout
    pollfd pollDescriptor = pollfd();
    pollDescriptor.fd = this->fileDescriptor;
    pollDescriptor.events = POLLIN;
    const int pollResult = ::poll(&pollDescriptor, 1, static_cast<int>(timeout.count()));
    if (pollResult == 0) {
        return SerialPortStatusType::TIMED_OUT;
    }

    // waiting must not fail unless a signal interrupted it
    if (pollResult < 0) {
        return errno == EINTR ? SerialPortStatusType::TIMED_OUT : SerialPortStatusType::READ_FAILED;
    }

    // device must not have hung up without pending data
    if ((pollDescriptor.revents & POLLIN) == 0
        && (pollDescriptor.revents & (POLLHUP | POLLERR)) != 0) {
        return SerialPortStatusType::DISCONNECTED;
    }

    // device must not have hung up while reading
    const ssize_t readResult = ::read(this->fileDescriptor, data, capacity);
    if (readResult == 0) {
        return SerialPortStatusType::DISCONNECTED;
    }

    // read must not fail unless no byte is waiting
    if (readResult < 0) {
        return errno == EAGAIN || errno == EINTR ? SerialPortStatusType::TIMED_OUT
                                                 : SerialPortStatusType::READ_FAILED;
    }

    // set the size of the read data
    size = static_cast<size_t>(readResult);

    // data read successfully
    return SerialPortStatusType::OK;
}

bool SerialPort::sendData(const uint8_t *data, const size_t size)
{
    // port must be open
    if (this->fileDescriptor < 0) {
        return false;
    }

    // write until every byte is sent
    size_t sentSize = 0;
    while (sentSize < size) {
        // write what the driver takes
        const ssize_t writeResult = ::write(this->fileDescriptor, data + sentSize, size - sentSize);
        if (writeResult > 0) {
            sentSize += static_cast<size_t>(writeResult);
            continue;
        }

        // write again after a signal
        if (writeResult < 0 && errno == EINTR) {
            continue;
        }

        // write must not fail unless the driver buffer is full
        if (writeResult < 0 && errno != EAGAIN) {
            return false;
        }

        // driver buffer must drain within the write timeout
        pollfd pollDescriptor = pollfd();
        pollDescriptor.fd = this->fileDescriptor;
        pollDescriptor.events = POLLOUT;
        const int pollResult = ::poll(&pollDescriptor,
                                      1,
                                      static_cast<int>(HostSettings::WRITE_TIMEOUT_MSEC.count()));
        if (pollResult <= 0) {
            return false;
        }
    }

    // data sent successfully
    return true;
}

bool SerialPort::getOpened() const
{
    // expose whether the port is open
    return this->fileDescriptor >= 0;
}

std::string SerialPort::getPortName() const
{
    // expose the port name
    return this->portName;
}

int SerialPort::getFileDescriptor() const
{
    // expose the file descriptor
    return this->fileDescriptor;
}

} // namespace sercom::SerialPort

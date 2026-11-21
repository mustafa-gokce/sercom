#ifndef SERCOM_SERIAL_PORT_H
#define SERCOM_SERIAL_PORT_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

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
    UNSUPPORTED_BAUD_RATE = 6, // serial port does not support the baud rate
    READ_FAILED = 7,           // reading from the device failed
    DISCONNECTED = 8,          // device was removed or became unavailable
    WRITE_FAILED = 9,          // writing to the device failed
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
 * Get the serial port status type of a QSerialPort error.
 */
[[nodiscard]] SerialPortStatusType getSerialPortStatusType(
    QSerialPort::SerialPortError serialPortError);

/*
 * Check whether the serial port supports the baud rate.
 */
[[nodiscard]] bool isBaudRateSupported(uint32_t baudRate);

class SerialPort final : public QObject, public sercom::Transport::Transport
{
    Q_OBJECT

public:
    /*
     * Serial port of the host on QSerialPort in raw 8N1 mode without flow control.
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
     * Open and configure a serial device, a pseudo terminal or a COM port.
     * Close an open port first.
     */
    [[nodiscard]] SerialPortStatusType open(const std::string &portName, uint32_t baudRate);

    /*
     * Send the bytes that wait for the driver and close the port.
     * Closing a closed port does nothing.
     */
    void close();

    /*
     * Hand every byte of the data to the driver, the event loop sends what the driver can not take yet.
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

signals:
    /*
     * Notify listeners about the received data.
     */
    void receivedData(const QByteArray &data);

    /*
     * Notify listeners about an error of the open port.
     */
    void errorHappened(sercom::SerialPort::SerialPortStatusType serialPortStatusType);

private slots:
    /*
     * Listen for received bytes that are ready to read.
     */
    void onReadyRead();

    /*
     * Listen for an error of the serial port.
     */
    void onErrorOccurred(QSerialPort::SerialPortError serialPortError);

private:
    QSerialPort serialPort; // serial port of Qt
    std::string portName;   // name of the open port
};

} // namespace sercom::SerialPort

#endif // SERCOM_SERIAL_PORT_H

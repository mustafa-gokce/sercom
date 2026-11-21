#ifndef SERCOM_SERIAL_PORT_H
#define SERCOM_SERIAL_PORT_H

#include <cstdint>
#include <string>

#include <QObject>
#include <QSerialPort>

namespace sercom::SerialPort {

class SerialPort final : public QObject
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
    [[nodiscard]] bool open(const std::string &portName, uint32_t baudRate);

    /*
     * Send the bytes that wait for the driver and close the port.
     * Closing a closed port does nothing.
     */
    void close();

public:
    /*
     * Get whether the port is open.
     */
    [[nodiscard]] bool getOpened() const;

    /*
     * Get the name of the open port.
     */
    [[nodiscard]] std::string getPortName() const;

private:
    QSerialPort serialPort; // serial port of Qt
    std::string portName;   // name of the open port
};

} // namespace sercom::SerialPort

#endif // SERCOM_SERIAL_PORT_H

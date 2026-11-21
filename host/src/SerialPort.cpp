#include "../include/SerialPort.h"

#include <QIODevice>
#include <QSignalBlocker>
#include <QString>

#include "../include/HostSettings.h"

namespace sercom::SerialPort {

SerialPort::SerialPort()
    : serialPort()
{
    // connect serial port signals and slots
    QObject::connect(&this->serialPort, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
}

SerialPort::~SerialPort()
{
    // close the device
    this->close();
}

bool SerialPort::open(const std::string &portName, const uint32_t baudRate)
{
    // close the open port
    this->close();

    // use raw 8N1 mode without flow control and a bounded read buffer
    this->serialPort.setPortName(QString::fromStdString(portName));
    this->serialPort.setBaudRate(static_cast<qint32>(baudRate));
    this->serialPort.setDataBits(QSerialPort::DataBits::Data8);
    this->serialPort.setParity(QSerialPort::Parity::NoParity);
    this->serialPort.setStopBits(QSerialPort::StopBits::OneStop);
    this->serialPort.setFlowControl(QSerialPort::FlowControl::NoFlowControl);
    this->serialPort.setReadBufferSize(static_cast<qint64>(HostSettings::READ_BUFFER_SIZE));

    // device must open for reading and writing
    if (!this->serialPort.open(QIODevice::OpenModeFlag::ReadWrite)) {
        return false;
    }

    // NOTE: toggling DTR or RTS resets many ESP32 boards
    [[maybe_unused]] const bool cleared = this->serialPort.clear(
        QSerialPort::Direction::AllDirections);

    // remember the open port
    this->portName = portName;

    // serial port opened successfully
    return true;
}

void SerialPort::close()
{
    // port must be open
    if (!this->serialPort.isOpen()) {
        return;
    }

    // send the bytes that wait for the driver within the write timeout without reading more
    {
        const QSignalBlocker signalBlocker(&this->serialPort);
        bool bytesWritten = true;
        while (bytesWritten && this->serialPort.bytesToWrite() > 0) {
            bytesWritten = this->serialPort.waitForBytesWritten(
                static_cast<int>(HostSettings::WRITE_TIMEOUT_MSEC.count()));
        }
    }

    // close the device
    this->serialPort.close();

    // forget the closed port
    this->portName.clear();
}

bool SerialPort::sendData(const uint8_t *data, const size_t size)
{
    // port must be open
    if (!this->serialPort.isOpen()) {
        return false;
    }

    // serial port must take every byte
    const qint64 dataSize = static_cast<qint64>(size);
    if (this->serialPort.write(reinterpret_cast<const char *>(data), dataSize) != dataSize) {
        return false;
    }

    // hand the bytes to the driver at once
    [[maybe_unused]] const bool flushed = this->serialPort.flush();

    // data sent successfully
    return true;
}

bool SerialPort::getOpened() const
{
    // expose whether the port is open
    return this->serialPort.isOpen();
}

std::string SerialPort::getPortName() const
{
    // expose the port name
    return this->portName;
}

void SerialPort::onReadyRead()
{
    // bytes must have arrived
    const QByteArray data = this->serialPort.readAll();
    if (data.isEmpty()) {
        return;
    }

    // notify listeners about the received data
    emit this->receivedData(data);
}

} // namespace sercom::SerialPort

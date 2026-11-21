#include "../include/SerialPort.h"

#include <algorithm>
#include <array>

#include <QIODevice>
#include <QSignalBlocker>
#include <QString>

#include "../include/HostSettings.h"

namespace sercom::SerialPort {

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
    case SerialPortStatusType::WRITE_FAILED: {
        return {"WRITE_FAILED"};
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
    if (serialPortStatusType == std::string_view("WRITE_FAILED")) {
        return SerialPortStatusType::WRITE_FAILED;
    }
    return SerialPortStatusType::UNKNOWN;
}

SerialPortStatusType getSerialPortStatusType(const QSerialPort::SerialPortError serialPortError)
{
    switch (serialPortError) {
    case QSerialPort::SerialPortError::NoError: {
        return SerialPortStatusType::OK;
    }
    case QSerialPort::SerialPortError::DeviceNotFoundError:
    case QSerialPort::SerialPortError::PermissionError:
    case QSerialPort::SerialPortError::OpenError: {
        return SerialPortStatusType::OPEN_FAILED;
    }
    case QSerialPort::SerialPortError::UnsupportedOperationError: {
        return SerialPortStatusType::CONFIGURATION_FAILED;
    }
    case QSerialPort::SerialPortError::ReadError: {
        return SerialPortStatusType::READ_FAILED;
    }
    case QSerialPort::SerialPortError::WriteError: {
        return SerialPortStatusType::WRITE_FAILED;
    }
    case QSerialPort::SerialPortError::ResourceError: {
        return SerialPortStatusType::DISCONNECTED;
    }
    case QSerialPort::SerialPortError::TimeoutError: {
        return SerialPortStatusType::TIMED_OUT;
    }
    case QSerialPort::SerialPortError::NotOpenError: {
        return SerialPortStatusType::NOT_OPEN;
    }
    case QSerialPort::SerialPortError::UnknownError:
    default: {
        return SerialPortStatusType::UNKNOWN;
    }
    }
}

bool isBaudRateSupported(const uint32_t baudRate)
{
    // expose whether the baud rate is one of the standard baud rates of every system
    static constexpr std::array<uint32_t, 8> supportedBaudRates
        = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    return std::find(supportedBaudRates.cbegin(), supportedBaudRates.cend(), baudRate)
           != supportedBaudRates.cend();
}

SerialPort::SerialPort()
    : serialPort()
{
    // connect serial port signals and slots
    QObject::connect(&this->serialPort, &QSerialPort::readyRead, this, &SerialPort::onReadyRead);
    QObject::connect(&this->serialPort,
                     &QSerialPort::errorOccurred,
                     this,
                     &SerialPort::onErrorOccurred);
}

SerialPort::~SerialPort()
{
    // close the device
    this->close();
}

SerialPortStatusType SerialPort::open(const std::string &portName, const uint32_t baudRate)
{
    // close the open port
    this->close();

    // baud rate must be supported
    if (!isBaudRateSupported(baudRate)) {
        return SerialPortStatusType::UNSUPPORTED_BAUD_RATE;
    }

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
        return getSerialPortStatusType(this->serialPort.error());
    }

    // NOTE: toggling DTR or RTS resets many ESP32 boards
    [[maybe_unused]] const bool cleared = this->serialPort.clear(
        QSerialPort::Direction::AllDirections);

    // remember the open port
    this->portName = portName;

    // serial port opened successfully
    return SerialPortStatusType::OK;
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

void SerialPort::onErrorOccurred(const QSerialPort::SerialPortError serialPortError)
{
    // error must belong to the open port, a failed open reports its status itself
    if (serialPortError == QSerialPort::SerialPortError::NoError || !this->serialPort.isOpen()) {
        return;
    }

    // notify listeners about the error
    emit this->errorHappened(getSerialPortStatusType(serialPortError));
}

namespace {

/*
 * Register the status type of the error signal for spies, variants and queued connections.
 */
void registerSerialPortMetaTypes()
{
    // register the status type and its pointer form with the Qt meta type system
    [[maybe_unused]] const int serialPortStatusTypeId = qRegisterMetaType<SerialPortStatusType>();
    [[maybe_unused]] const int serialPortStatusTypePointerId
        = qRegisterMetaType<SerialPortStatusType *>();
}

// ensure the types are registered at startup
Q_CONSTRUCTOR_FUNCTION(registerSerialPortMetaTypes)

} // namespace

} // namespace sercom::SerialPort

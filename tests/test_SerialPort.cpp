#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <QMetaType>
#include <QSerialPort>
#include <QSignalSpy>

#include <catch2/catch_all.hpp>

#include "../host/include/HostSettings.h"
#include "../host/include/SerialPort.h"

using namespace sercom;

class TestSerialPort
{
public:
    /*
     * Serial port test fixture.
     */
    TestSerialPort()
    {
        // initialize serial port
        this->serialPort.reset(new SerialPort::SerialPort());
    }
    ~TestSerialPort()
    {
        // close serial port
        this->serialPort.reset(nullptr);
    }

    // delete copy and move operators
    TestSerialPort(const TestSerialPort &other) = delete;
    TestSerialPort &operator=(const TestSerialPort &other) = delete;
    TestSerialPort(TestSerialPort &&other) noexcept = delete;
    TestSerialPort &operator=(TestSerialPort &&other) noexcept = delete;

protected:
    std::unique_ptr<SerialPort::SerialPort> serialPort; // serial port under test
};

TEST_CASE_METHOD(TestSerialPort,
                 "Serial port status type can be converted to string",
                 "[SerialPort][fromSerialPortStatusType]")
{
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::UNKNOWN)
            == "UNKNOWN");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::OK) == "OK");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::TIMED_OUT)
            == "TIMED_OUT");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::NOT_OPEN)
            == "NOT_OPEN");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::OPEN_FAILED)
            == "OPEN_FAILED");
    REQUIRE(
        SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::CONFIGURATION_FAILED)
        == "CONFIGURATION_FAILED");
    REQUIRE(SerialPort::fromSerialPortStatusType(
                SerialPort::SerialPortStatusType::UNSUPPORTED_BAUD_RATE)
            == "UNSUPPORTED_BAUD_RATE");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::READ_FAILED)
            == "READ_FAILED");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::DISCONNECTED)
            == "DISCONNECTED");
    REQUIRE(SerialPort::fromSerialPortStatusType(SerialPort::SerialPortStatusType::WRITE_FAILED)
            == "WRITE_FAILED");
}

TEST_CASE_METHOD(TestSerialPort,
                 "Serial port status type can be parsed from string",
                 "[SerialPort][toSerialPortStatusType]")
{
    REQUIRE(SerialPort::toSerialPortStatusType("UNKNOWN")
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::toSerialPortStatusType("OK") == SerialPort::SerialPortStatusType::OK);
    REQUIRE(SerialPort::toSerialPortStatusType("TIMED_OUT")
            == SerialPort::SerialPortStatusType::TIMED_OUT);
    REQUIRE(SerialPort::toSerialPortStatusType("NOT_OPEN")
            == SerialPort::SerialPortStatusType::NOT_OPEN);
    REQUIRE(SerialPort::toSerialPortStatusType("OPEN_FAILED")
            == SerialPort::SerialPortStatusType::OPEN_FAILED);
    REQUIRE(SerialPort::toSerialPortStatusType("CONFIGURATION_FAILED")
            == SerialPort::SerialPortStatusType::CONFIGURATION_FAILED);
    REQUIRE(SerialPort::toSerialPortStatusType("UNSUPPORTED_BAUD_RATE")
            == SerialPort::SerialPortStatusType::UNSUPPORTED_BAUD_RATE);
    REQUIRE(SerialPort::toSerialPortStatusType("READ_FAILED")
            == SerialPort::SerialPortStatusType::READ_FAILED);
    REQUIRE(SerialPort::toSerialPortStatusType("DISCONNECTED")
            == SerialPort::SerialPortStatusType::DISCONNECTED);
    REQUIRE(SerialPort::toSerialPortStatusType("WRITE_FAILED")
            == SerialPort::SerialPortStatusType::WRITE_FAILED);
    REQUIRE(SerialPort::toSerialPortStatusType("LOREM_IPSUM")
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::toSerialPortStatusType("open_failed")
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::toSerialPortStatusType("OPEN_FAILED ")
            == SerialPort::SerialPortStatusType::UNKNOWN);
}

TEST_CASE_METHOD(TestSerialPort,
                 "A QSerialPort error gives its serial port status type",
                 "[SerialPort][getSerialPortStatusType]")
{
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::NoError)
            == SerialPort::SerialPortStatusType::OK);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::DeviceNotFoundError)
            == SerialPort::SerialPortStatusType::OPEN_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::PermissionError)
            == SerialPort::SerialPortStatusType::OPEN_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::OpenError)
            == SerialPort::SerialPortStatusType::OPEN_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::ParityError)
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::FramingError)
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::BreakConditionError)
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::WriteError)
            == SerialPort::SerialPortStatusType::WRITE_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::ReadError)
            == SerialPort::SerialPortStatusType::READ_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::ResourceError)
            == SerialPort::SerialPortStatusType::DISCONNECTED);
    REQUIRE(
        SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::UnsupportedOperationError)
        == SerialPort::SerialPortStatusType::CONFIGURATION_FAILED);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::UnknownError)
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::TimeoutError)
            == SerialPort::SerialPortStatusType::TIMED_OUT);
    REQUIRE(SerialPort::getSerialPortStatusType(QSerialPort::SerialPortError::NotOpenError)
            == SerialPort::SerialPortStatusType::NOT_OPEN);
}

TEST_CASE_METHOD(TestSerialPort,
                 "Only the standard baud rates from 9600 to 921600 are supported",
                 "[SerialPort][testIsBaudRateSupported]")
{
    // check every supported baud rate
    static const std::vector<uint32_t> supportedBaudRates
        = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    std::for_each(supportedBaudRates.cbegin(),
                  supportedBaudRates.cend(),
                  [](const uint32_t supportedBaudRate) {
                      CAPTURE(supportedBaudRate);
                      REQUIRE(SerialPort::isBaudRateSupported(supportedBaudRate));
                  });

    // check baud rates outside the supported ones
    static const std::vector<uint32_t> unsupportedBaudRates = {0, 1, 1234, 9601, 115201};
    std::for_each(unsupportedBaudRates.cbegin(),
                  unsupportedBaudRates.cend(),
                  [](const uint32_t unsupportedBaudRate) {
                      CAPTURE(unsupportedBaudRate);
                      REQUIRE_FALSE(SerialPort::isBaudRateSupported(unsupportedBaudRate));
                  });
}

TEST_CASE_METHOD(TestSerialPort,
                 "A closed serial port sends nothing",
                 "[SerialPort][testClosedPort]")
{
    // check the state of a port that was never opened
    REQUIRE_FALSE(serialPort->getOpened());
    REQUIRE(serialPort->getPortName().empty());

    // check a send without an open port
    const std::vector<uint8_t> data = {0xA5, 0xC3};
    REQUIRE_FALSE(serialPort->sendData(data.data(), data.size()));

    // check that closing a closed port does nothing
    serialPort->close();
    REQUIRE_FALSE(serialPort->getOpened());
}

TEST_CASE_METHOD(TestSerialPort,
                 "A missing serial port and an unsupported baud rate do not open",
                 "[SerialPort][testOpen]")
{
    // check that the status type of the error signal is known by its name
    REQUIRE_FALSE(QMetaType::type("sercom::SerialPort::SerialPortStatusType")
                  == QMetaType::UnknownType);

    // check that the spy listens for the error signal
    QSignalSpy errorHappenedSpy(serialPort.get(), &SerialPort::SerialPort::errorHappened);
    REQUIRE(errorHappenedSpy.isValid());

    // check a port name that names no device on any system
    const std::string missingPortName = "sercom-missing-port";
    REQUIRE(serialPort->open(missingPortName, HostSettings::DEFAULT_BAUD_RATE)
            == SerialPort::SerialPortStatusType::OPEN_FAILED);
    REQUIRE_FALSE(serialPort->getOpened());
    REQUIRE(serialPort->getPortName().empty());

    // check that the failed open reports its status only through its result
    REQUIRE(errorHappenedSpy.count() == 0);

    // check a baud rate that is not supported before the device is touched
    static constexpr uint32_t unsupportedBaudRate = 1234;
    REQUIRE(serialPort->open(missingPortName, unsupportedBaudRate)
            == SerialPort::SerialPortStatusType::UNSUPPORTED_BAUD_RATE);
    REQUIRE_FALSE(serialPort->getOpened());

    // check that the spy records an error signal with its status type
    emit serialPort->errorHappened(SerialPort::SerialPortStatusType::DISCONNECTED);
    REQUIRE(errorHappenedSpy.count() == 1);
    REQUIRE(errorHappenedSpy.at(0).at(0).value<SerialPort::SerialPortStatusType>()
            == SerialPort::SerialPortStatusType::DISCONNECTED);
}

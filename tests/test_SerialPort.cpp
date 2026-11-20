#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../host/include/SerialPort.h"

using namespace sercom;

class TestSerialPort
{
public:
    /*
     * Serial port test fixture.
     */
    TestSerialPort() = default;
    ~TestSerialPort() = default;

    // delete copy and move operators
    TestSerialPort(const TestSerialPort &other) = delete;
    TestSerialPort &operator=(const TestSerialPort &other) = delete;
    TestSerialPort(TestSerialPort &&other) noexcept = delete;
    TestSerialPort &operator=(TestSerialPort &&other) noexcept = delete;
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
    REQUIRE(SerialPort::toSerialPortStatusType("LOREM_IPSUM")
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::toSerialPortStatusType("open_failed")
            == SerialPort::SerialPortStatusType::UNKNOWN);
    REQUIRE(SerialPort::toSerialPortStatusType("OPEN_FAILED ")
            == SerialPort::SerialPortStatusType::UNKNOWN);
}

TEST_CASE_METHOD(TestSerialPort,
                 "Only baud rates with a termios speed are supported",
                 "[SerialPort][testIsBaudRateSupported]")
{
    // check every baud rate with a termios speed
    static const std::vector<uint32_t> supportedBaudRates
        = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    std::for_each(supportedBaudRates.cbegin(),
                  supportedBaudRates.cend(),
                  [](const uint32_t supportedBaudRate) {
                      CAPTURE(supportedBaudRate);
                      REQUIRE(SerialPort::isBaudRateSupported(supportedBaudRate));
                  });

    // check baud rates without a termios speed
    static const std::vector<uint32_t> unsupportedBaudRates = {0, 1, 1234, 9601, 115201};
    std::for_each(unsupportedBaudRates.cbegin(),
                  unsupportedBaudRates.cend(),
                  [](const uint32_t unsupportedBaudRate) {
                      CAPTURE(unsupportedBaudRate);
                      REQUIRE_FALSE(SerialPort::isBaudRateSupported(unsupportedBaudRate));
                  });
}

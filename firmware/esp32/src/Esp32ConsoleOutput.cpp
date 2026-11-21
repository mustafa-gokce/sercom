#include "../include/Esp32ConsoleOutput.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>

#include "../../../app/include/AppSettings.h"
#include "../../common/include/CommonSettings.h"

#include "../include/FirmwareSettings.h"

namespace sercom::Esp32ConsoleOutput {

Esp32ConsoleOutput::Esp32ConsoleOutput(HardwareSerial &serial, const bool printed)
    : serial(serial)
    , printed(printed)
    , streamBuffer(nullptr)
{}

Esp32ConsoleOutput::~Esp32ConsoleOutput() {}

bool Esp32ConsoleOutput::begin()
{
    // console output must be printed
    if (!this->printed) {
        return false;
    }

    // console buffer must be created
    this->streamBuffer = xStreamBufferCreate(FirmwareSettings::CONSOLE_TRANSMIT_BUFFER_SIZE,
                                             FirmwareSettings::CONSOLE_STREAM_TRIGGER_SIZE);
    if (this->streamBuffer == nullptr) {
        return false;
    }

    // open the serial port 8N1 without flow control
    this->serial.begin(CommonSettings::CONSOLE_BAUD_RATE, SERIAL_8N1);

    // print the name and the version of the device
    std::array<char, CommonSettings::CONSOLE_LINE_SIZE> line
        = std::array<char, CommonSettings::CONSOLE_LINE_SIZE>();
    const int nameLineLength = std::snprintf(line.data(),
                                             line.size(),
                                             "\r\n%.*s version %.*s git %.*s\r\n",
                                             static_cast<int>(FirmwareSettings::DEVICE_NAME.size()),
                                             FirmwareSettings::DEVICE_NAME.data(),
                                             static_cast<int>(AppSettings::AppVersionString.size()),
                                             AppSettings::AppVersionString.data(),
                                             static_cast<int>(AppSettings::AppGitHash.size()),
                                             AppSettings::AppGitHash.data());
    this->writeLine(line.data(), nameLineLength);

    // print the serial port of the protocol
    const int protocolLineLength
        = std::snprintf(line.data(),
                        line.size(),
                        "protocol on %.*s at %" PRIu32
                        " baud, receive pin GPIO%d, transmit pin GPIO%d\r\n",
                        static_cast<int>(FirmwareSettings::SerialPortName.size()),
                        FirmwareSettings::SerialPortName.data(),
                        CommonSettings::SERIAL_BAUD_RATE,
                        static_cast<int>(FirmwareSettings::SerialReceivePin),
                        static_cast<int>(FirmwareSettings::SerialTransmitPin));
    this->writeLine(line.data(), protocolLineLength);

    // print the serial port of the console
    const int consoleLineLength = std::snprintf(line.data(),
                                                line.size(),
                                                "console on %.*s at %" PRIu32 " baud\r\n",
                                                static_cast<int>(
                                                    FirmwareSettings::UART0_PORT_NAME.size()),
                                                FirmwareSettings::UART0_PORT_NAME.data(),
                                                CommonSettings::CONSOLE_BAUD_RATE);
    this->writeLine(line.data(), consoleLineLength);

    // console output opened successfully
    return true;
}

void Esp32ConsoleOutput::processConsole()
{
    // console buffer must exist
    if (this->streamBuffer == nullptr) {
        return;
    }

    // wait for the next bytes in the console buffer
    std::array<char, FirmwareSettings::CONSOLE_CHUNK_SIZE> chunk
        = std::array<char, FirmwareSettings::CONSOLE_CHUNK_SIZE>();
    const size_t chunkSize = xStreamBufferReceive(this->streamBuffer,
                                                  chunk.data(),
                                                  chunk.size(),
                                                  portMAX_DELAY);

    // write the bytes to the serial port
    [[maybe_unused]] const size_t writtenSize = this->serial.write(chunk.data(), chunkSize);
}

size_t Esp32ConsoleOutput::getAvailableSize() const
{
    // console buffer must exist
    if (this->streamBuffer == nullptr) {
        return 0;
    }

    // expose the free space of the console buffer
    return xStreamBufferSpacesAvailable(this->streamBuffer);
}

void Esp32ConsoleOutput::writeText(const char *text, const size_t size)
{
    // console buffer must exist
    if (this->streamBuffer == nullptr) {
        return;
    }

    // NOTE: a stream buffer asserts on a write of zero bytes
    if (size == 0) {
        return;
    }

    // queue the text for the console task
    [[maybe_unused]] const size_t queuedSize
        = xStreamBufferSend(this->streamBuffer,
                            text,
                            size,
                            pdMS_TO_TICKS(FirmwareSettings::CONSOLE_STREAM_TIMEOUT_MSEC.count()));
}

void Esp32ConsoleOutput::writeLine(const char *line, const int lineLength)
{
    // line must be formatted
    if (lineLength <= 0) {
        return;
    }

    // write the line without its terminator
    this->writeText(line,
                    std::min(static_cast<size_t>(lineLength),
                             CommonSettings::CONSOLE_LINE_SIZE - 1));
}

} // namespace sercom::Esp32ConsoleOutput

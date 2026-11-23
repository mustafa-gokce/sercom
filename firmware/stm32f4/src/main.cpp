#include "../include/main.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <stm32f4xx.h>

#include "../../../app/include/AppSettings.h"
#include "../../../app/include/Messages.h"
#include "../../common/include/CommonSettings.h"
#include "../../common/include/Console.h"
#include "../../common/include/ControllerRole.h"
#include "../../common/include/DeviceRole.h"
#include "../../common/include/FirmwareRole.h"

#include "../include/FirmwareSettings.h"
#include "../include/Stm32Clock.h"
#include "../include/Stm32Platform.h"
#include "../include/UsartPort.h"

/*
 * Global variables.
 */
static sercom::Stm32Clock::Stm32Clock stm32Clock; // millisecond clock of the SysTick timer

// initialize device
static sercom::UsartPort::UsartPort usart1Port(USART1,
                                               USART1_IRQn,
                                               sercom::FirmwareSettings::USART1_RECEIVE_PIN,
                                               sercom::FirmwareSettings::USART1_TRANSMIT_PIN);
static sercom::UsartPort::UsartPort usart2Port(USART2,
                                               USART2_IRQn,
                                               sercom::FirmwareSettings::USART2_RECEIVE_PIN,
                                               sercom::FirmwareSettings::USART2_TRANSMIT_PIN);
static sercom::UsartPort::UsartPort &protocolPort = sercom::FirmwareSettings::ConsolePrinted
                                                        ? usart1Port
                                                        : usart2Port;
static sercom::UsartPort::UsartPort &consolePort = sercom::FirmwareSettings::ConsolePrinted
                                                       ? usart2Port
                                                       : usart1Port;
static sercom::Stm32Platform::Stm32Platform stm32Platform(stm32Clock,
                                                          sercom::FirmwareSettings::DEVICE_NAME);
static sercom::Console::Console console(consolePort,
                                        stm32Clock,
                                        sercom::FirmwareSettings::ConsolePrinted);
static sercom::DeviceRole::DeviceRole deviceRole(protocolPort, stm32Clock, stm32Platform, console);
static sercom::ControllerRole::ControllerRole controllerRole(protocolPort,
                                                             stm32Clock,
                                                             stm32Platform,
                                                             console);
static sercom::FirmwareRole::FirmwareRole &firmwareRole
    = sercom::FirmwareSettings::ControllerRun
          ? static_cast<sercom::FirmwareRole::FirmwareRole &>(controllerRole)
          : static_cast<sercom::FirmwareRole::FirmwareRole &>(deviceRole);

int main()
{
    using namespace sercom;

    // configure the board and paint the free memory
    stm32Platform.begin();

    // start the millisecond clock
    stm32Clock.begin();

    // open the console of a printed console and print the boot banner
    if (FirmwareSettings::ConsolePrinted) {
        consolePort.begin(CommonSettings::CONSOLE_BAUD_RATE);
        printBanner();
    }

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> startingDebugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int startingDebugMessageLength
        = std::snprintf(startingDebugMessage.data(),
                        startingDebugMessage.size(),
                        "Starting the %.*s on %.*s, %" PRIu32,
                        static_cast<int>(FirmwareSettings::ApplicationName.size()),
                        FirmwareSettings::ApplicationName.data(),
                        static_cast<int>(FirmwareSettings::SerialPortName.size()),
                        FirmwareSettings::SerialPortName.data(),
                        CommonSettings::SERIAL_BAUD_RATE);
    firmwareRole.reportLog(Messages::LogLevelType::DEBUG, startingDebugMessage.data());

    // open the serial port 8N1 without flow control on its pins
    protocolPort.begin(CommonSettings::SERIAL_BAUD_RATE);

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> startedDebugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int startedDebugMessageLength
        = std::snprintf(startedDebugMessage.data(),
                        startedDebugMessage.size(),
                        "Started the %.*s on %.*s, %" PRIu32 " for the device %.*s version %.*s",
                        static_cast<int>(FirmwareSettings::ApplicationName.size()),
                        FirmwareSettings::ApplicationName.data(),
                        static_cast<int>(FirmwareSettings::SerialPortName.size()),
                        FirmwareSettings::SerialPortName.data(),
                        CommonSettings::SERIAL_BAUD_RATE,
                        static_cast<int>(FirmwareSettings::DEVICE_NAME.size()),
                        FirmwareSettings::DEVICE_NAME.data(),
                        static_cast<int>(AppSettings::AppVersionString.size()),
                        AppSettings::AppVersionString.data());
    firmwareRole.reportLog(Messages::LogLevelType::DEBUG, startedDebugMessage.data());

    // run the firmware role in the superloop
    std::array<uint8_t, CommonSettings::PROTOCOL_CHUNK_SIZE> chunk
        = std::array<uint8_t, CommonSettings::PROTOCOL_CHUNK_SIZE>();
    std::chrono::milliseconds lastHeartbeatTimestamp = stm32Clock.getTime();
    while (true) {
        // feed the received bytes to the firmware role
        const size_t receivedSize = protocolPort.receiveData(chunk.data(), chunk.size());
        if (receivedSize > 0) {
            firmwareRole.onReceivedData(chunk.data(), receivedSize);
        }

        // report the health of the device periodically
        const std::chrono::milliseconds currentTime = stm32Clock.getTime();
        if (currentTime - lastHeartbeatTimestamp >= CommonSettings::HEARTBEAT_PERIOD_MSEC) {
            lastHeartbeatTimestamp = currentTime;
            reportHeartbeat();
        }

        // maintain the firmware role
        firmwareRole.maintain();

        // sleep until the next interrupt, the SysTick timer wakes the loop every millisecond
        __WFI();
    }
}

void printBanner()
{
    using namespace sercom;

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
    writeConsoleLine(line.data(), nameLineLength);

    // print the serial port of the protocol
    const int protocolLineLength = std::snprintf(line.data(),
                                                 line.size(),
                                                 "protocol on %.*s at %" PRIu32
                                                 " baud, receive pin PA%" PRIu32
                                                 ", transmit pin PA%" PRIu32 "\r\n",
                                                 static_cast<int>(
                                                     FirmwareSettings::SerialPortName.size()),
                                                 FirmwareSettings::SerialPortName.data(),
                                                 CommonSettings::SERIAL_BAUD_RATE,
                                                 FirmwareSettings::SerialReceivePin,
                                                 FirmwareSettings::SerialTransmitPin);
    writeConsoleLine(line.data(), protocolLineLength);

    // print the serial port of the console
    const int consoleLineLength = std::snprintf(line.data(),
                                                line.size(),
                                                "console on %.*s at %" PRIu32 " baud\r\n",
                                                static_cast<int>(
                                                    FirmwareSettings::USART2_PORT_NAME.size()),
                                                FirmwareSettings::USART2_PORT_NAME.data(),
                                                CommonSettings::CONSOLE_BAUD_RATE);
    writeConsoleLine(line.data(), consoleLineLength);
}

void writeConsoleLine(const char *line, const int lineLength)
{
    using namespace sercom;

    // line must be formatted
    if (lineLength <= 0) {
        return;
    }

    // write the line without its terminator
    consolePort.writeText(line,
                          std::min(static_cast<size_t>(lineLength),
                                   CommonSettings::CONSOLE_LINE_SIZE - 1));
}

void reportHeartbeat()
{
    using namespace sercom;

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> debugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int debugMessageLength
        = std::snprintf(debugMessage.data(),
                        debugMessage.size(),
                        "Heartbeat: free memory %" PRIu32 " bytes, dropped received bytes %" PRIu32,
                        stm32Platform.getFreeHeapSize(),
                        protocolPort.getDroppedByteCounter());
    firmwareRole.reportLog(Messages::LogLevelType::DEBUG, debugMessage.data());
}

void SysTick_Handler()
{
    // count one millisecond
    stm32Clock.processTick();
}

void USART1_IRQHandler()
{
    // serve the interrupt of USART1
    usart1Port.processInterrupt();
}

void USART2_IRQHandler()
{
    // serve the interrupt of USART2
    usart2Port.processInterrupt();
}

void *_sbrk([[maybe_unused]] const ptrdiff_t increment)
{
    // NOTE: newlib links malloc for snprintf, every request of it fails here
    static constexpr intptr_t failedBreak = -1;
    errno = ENOMEM;
    return reinterpret_cast<void *>(failedBreak);
}

int _close([[maybe_unused]] const int file)
{
    // NOTE: the exit of newlib links the stdio streams, every close request of them fails here
    static constexpr int failedClose = -1;
    errno = EBADF;
    return failedClose;
}

off_t _lseek([[maybe_unused]] const int file,
             [[maybe_unused]] const off_t offset,
             [[maybe_unused]] const int origin)
{
    // NOTE: the exit of newlib links the stdio streams, every seek request of them fails here
    static constexpr off_t failedSeek = -1;
    errno = EBADF;
    return failedSeek;
}

ssize_t _read([[maybe_unused]] const int file,
              [[maybe_unused]] void *buffer,
              [[maybe_unused]] const size_t size)
{
    // NOTE: the exit of newlib links the stdio streams, every read request of them fails here
    static constexpr ssize_t failedRead = -1;
    errno = EBADF;
    return failedRead;
}

ssize_t _write([[maybe_unused]] const int file,
               [[maybe_unused]] const void *buffer,
               [[maybe_unused]] const size_t size)
{
    // NOTE: the exit of newlib links the stdio streams, every write request of them fails here
    static constexpr ssize_t failedWrite = -1;
    errno = EBADF;
    return failedWrite;
}

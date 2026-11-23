#include "../include/main.h"

#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include <Arduino.h>

#include "../../../app/include/AppSettings.h"
#include "../../../app/include/Messages.h"
#include "../../../firmware/common/include/CommonSettings.h"
#include "../../../firmware/common/include/Console.h"
#include "../../../firmware/common/include/DeviceRole.h"

#include "../include/ConsumerPlatform.h"
#include "../include/ConsumerPort.h"

// initialize device
static sercom::ConsumerPort::ConsumerPort consumerPort(Serial);
static sercom::ConsumerPlatform::ConsumerPlatform consumerPlatform(
    sercom::ConsumerPlatform::LED_PIN, sercom::ConsumerPlatform::DEVICE_NAME);
static sercom::Console::Console console(consumerPort,
                                        consumerPlatform,
                                        sercom::ConsumerPort::CONSOLE_PRINTED);
static sercom::DeviceRole::DeviceRole deviceRole(consumerPort,
                                                 consumerPlatform,
                                                 consumerPlatform,
                                                 console);

void setup()
{
    using namespace sercom;

    // open the serial port of the protocol and configure the board
    consumerPort.begin();
    consumerPlatform.begin();

    // notify the user
    std::array<char, CommonSettings::LOG_LINE_SIZE> startedDebugMessage
        = std::array<char, CommonSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int startedDebugMessageLength
        = std::snprintf(startedDebugMessage.data(),
                        startedDebugMessage.size(),
                        "Started the DeviceRole on %.*s, %" PRIu32
                        " for the device %.*s version %.*s",
                        static_cast<int>(ConsumerPort::PORT_NAME.size()),
                        ConsumerPort::PORT_NAME.data(),
                        CommonSettings::SERIAL_BAUD_RATE,
                        static_cast<int>(ConsumerPlatform::DEVICE_NAME.size()),
                        ConsumerPlatform::DEVICE_NAME.data(),
                        static_cast<int>(AppSettings::AppVersionString.size()),
                        AppSettings::AppVersionString.data());
    deviceRole.reportLog(Messages::LogLevelType::DEBUG, startedDebugMessage.data());
}

void loop()
{
    using namespace sercom;

    // feed the received bytes to the device role
    std::array<uint8_t, CommonSettings::PROTOCOL_CHUNK_SIZE> chunk
        = std::array<uint8_t, CommonSettings::PROTOCOL_CHUNK_SIZE>();
    const size_t receivedSize = consumerPort.receiveData(chunk.data(), chunk.size());
    if (receivedSize > 0) {
        deviceRole.onReceivedData(chunk.data(), receivedSize);
    }

    // maintain the device role
    deviceRole.maintain();
}

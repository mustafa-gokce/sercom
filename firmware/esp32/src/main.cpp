#include "../include/main.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <Arduino.h>

#include "../../../app/include/DeviceApplication.h"

#include "../include/Esp32Clock.h"
#include "../include/Esp32Platform.h"
#include "../include/FirmwareSettings.h"
#include "../include/UartTransport.h"

// initialize device
static sercom::UartTransport::UartTransport uartTransport(Serial);
static sercom::Esp32Clock::Esp32Clock esp32Clock;
static sercom::Esp32Platform::Esp32Platform esp32Platform(sercom::FirmwareSettings::LED_PIN,
                                                          sercom::FirmwareSettings::DEVICE_NAME);
static sercom::DeviceApplication::DeviceApplication deviceApplication(uartTransport,
                                                                      esp32Clock,
                                                                      esp32Platform);

void setup()
{
    using namespace sercom;

    // set the driver buffer sizes before the serial port begins
    Serial.setRxBufferSize(FirmwareSettings::SERIAL_RECEIVE_BUFFER_SIZE);
    Serial.setTxBufferSize(FirmwareSettings::SERIAL_TRANSMIT_BUFFER_SIZE);

    // open the serial port 8N1 without flow control
    Serial.begin(FirmwareSettings::SERIAL_BAUD_RATE, SERIAL_8N1);

    // configure the board
    esp32Platform.begin();
}

void loop()
{
    using namespace sercom;

    // received bytes of at most one chunk must be read
    std::array<uint8_t, FirmwareSettings::RECEIVE_CHUNK_SIZE> chunk
        = std::array<uint8_t, FirmwareSettings::RECEIVE_CHUNK_SIZE>();
    const int availableSize = Serial.available();
    if (availableSize > 0) {
        // hand the bytes to the device application
        const size_t requestedSize = std::min(static_cast<size_t>(availableSize), chunk.size());
        const size_t receivedSize = Serial.readBytes(chunk.data(), requestedSize);
        deviceApplication.onReceivedData(chunk.data(), receivedSize);
    }

    // maintain the device application
    deviceApplication.maintain();
}

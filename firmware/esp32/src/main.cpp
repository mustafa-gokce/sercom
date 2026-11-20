#include "../include/main.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "../../../app/include/DeviceApplication.h"

#include "../include/Esp32Clock.h"
#include "../include/Esp32Platform.h"
#include "../include/FirmwareSettings.h"
#include "../include/UartTransport.h"

/*
 * Global variables.
 */
static StreamBufferHandle_t receiveStreamBuffer = nullptr; // bytes from the receive task

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

    // create the channel between the tasks
    receiveStreamBuffer = xStreamBufferCreate(FirmwareSettings::RECEIVE_STREAM_BUFFER_SIZE,
                                              FirmwareSettings::RECEIVE_STREAM_TRIGGER_SIZE);

    // set the driver buffer sizes before the serial port begins
    Serial.setRxBufferSize(FirmwareSettings::SERIAL_RECEIVE_BUFFER_SIZE);
    Serial.setTxBufferSize(FirmwareSettings::SERIAL_TRANSMIT_BUFFER_SIZE);

    // open the serial port 8N1 without flow control
    Serial.begin(FirmwareSettings::SERIAL_BAUD_RATE, SERIAL_8N1);

    // configure the board
    esp32Platform.begin();

    // start the tasks
    xTaskCreatePinnedToCore(protocolTask,
                            "protocol",
                            FirmwareSettings::PROTOCOL_TASK_STACK_SIZE,
                            nullptr,
                            FirmwareSettings::PROTOCOL_TASK_PRIORITY,
                            nullptr,
                            FirmwareSettings::APPLICATION_CORE);
    xTaskCreatePinnedToCore(uartReceiveTask,
                            "uart_receive",
                            FirmwareSettings::UART_RECEIVE_TASK_STACK_SIZE,
                            nullptr,
                            FirmwareSettings::UART_RECEIVE_TASK_PRIORITY,
                            nullptr,
                            FirmwareSettings::APPLICATION_CORE);
}

void loop()
{
    // delete the loop task of the framework
    vTaskDelete(nullptr);
}

void uartReceiveTask([[maybe_unused]] void *parameter)
{
    using namespace sercom;

    // move the received bytes to the protocol task
    std::array<uint8_t, FirmwareSettings::RECEIVE_CHUNK_SIZE> chunk
        = std::array<uint8_t, FirmwareSettings::RECEIVE_CHUNK_SIZE>();
    while (true) {
        // received bytes must be available
        const int availableSize = Serial.available();
        if (availableSize <= 0) {
            vTaskDelay(pdMS_TO_TICKS(FirmwareSettings::UART_IDLE_DELAY_MSEC.count()));
            continue;
        }

        // received bytes of at most one chunk must be read
        const size_t requestedSize = std::min(static_cast<size_t>(availableSize), chunk.size());
        const size_t receivedSize = Serial.readBytes(chunk.data(), requestedSize);
        if (receivedSize == 0) {
            continue;
        }

        // hand the bytes to the protocol task
        [[maybe_unused]] const size_t sentSize = xStreamBufferSend(receiveStreamBuffer,
                                                                   chunk.data(),
                                                                   receivedSize,
                                                                   portMAX_DELAY);
    }
}

void protocolTask([[maybe_unused]] void *parameter)
{
    using namespace sercom;

    // NOTE: only this task may touch the device application
    std::array<uint8_t, FirmwareSettings::PROTOCOL_CHUNK_SIZE> chunk
        = std::array<uint8_t, FirmwareSettings::PROTOCOL_CHUNK_SIZE>();
    while (true) {
        // feed the received bytes to the device application
        const size_t receivedSize
            = xStreamBufferReceive(receiveStreamBuffer,
                                   chunk.data(),
                                   chunk.size(),
                                   pdMS_TO_TICKS(
                                       FirmwareSettings::PROTOCOL_TICK_PERIOD_MSEC.count()));
        if (receivedSize > 0) {
            deviceApplication.onReceivedData(chunk.data(), receivedSize);
        }

        // maintain the device application
        deviceApplication.maintain();
    }
}

#include "../include/main.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include "../../../app/include/AppSettings.h"
#include "../../../app/include/DeviceApplication.h"
#include "../../../app/include/Messages.h"

#include "../include/Esp32Clock.h"
#include "../include/Esp32Platform.h"
#include "../include/FirmwareSettings.h"
#include "../include/LogLine.h"
#include "../include/UartTransport.h"

/*
 * Global variables.
 */
static StreamBufferHandle_t receiveStreamBuffer = nullptr; // bytes from the receive task
static QueueHandle_t logQueue = nullptr;                   // log lines from any task
static TaskHandle_t protocolTaskHandle = nullptr;          // protocol task for stack reports

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

    // create the channels between the tasks
    receiveStreamBuffer = xStreamBufferCreate(FirmwareSettings::RECEIVE_STREAM_BUFFER_SIZE,
                                              FirmwareSettings::RECEIVE_STREAM_TRIGGER_SIZE);
    logQueue = xQueueCreate(FirmwareSettings::LOG_QUEUE_LENGTH, sizeof(LogLine::LogLine));

    // notify the user
    std::array<char, FirmwareSettings::LOG_LINE_SIZE> startingDebugMessage
        = std::array<char, FirmwareSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int startingDebugMessageLength
        = std::snprintf(startingDebugMessage.data(),
                        startingDebugMessage.size(),
                        "Starting the DeviceApplication on %.*s, %" PRIu32,
                        static_cast<int>(FirmwareSettings::SerialPortName.size()),
                        FirmwareSettings::SerialPortName.data(),
                        FirmwareSettings::SERIAL_BAUD_RATE);
    queueLog(Messages::LogLevelType::DEBUG, startingDebugMessage.data());

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
                            &protocolTaskHandle,
                            FirmwareSettings::APPLICATION_CORE);
    xTaskCreatePinnedToCore(uartReceiveTask,
                            "uart_receive",
                            FirmwareSettings::UART_RECEIVE_TASK_STACK_SIZE,
                            nullptr,
                            FirmwareSettings::UART_RECEIVE_TASK_PRIORITY,
                            nullptr,
                            FirmwareSettings::APPLICATION_CORE);
    xTaskCreatePinnedToCore(heartbeatTask,
                            "heartbeat",
                            FirmwareSettings::HEARTBEAT_TASK_STACK_SIZE,
                            nullptr,
                            FirmwareSettings::HEARTBEAT_TASK_PRIORITY,
                            nullptr,
                            FirmwareSettings::APPLICATION_CORE);

    // notify the user
    std::array<char, FirmwareSettings::LOG_LINE_SIZE> startedDebugMessage
        = std::array<char, FirmwareSettings::LOG_LINE_SIZE>();
    [[maybe_unused]] const int startedDebugMessageLength
        = std::snprintf(startedDebugMessage.data(),
                        startedDebugMessage.size(),
                        "Started the DeviceApplication on %.*s, %" PRIu32
                        " for the device %.*s version %.*s",
                        static_cast<int>(FirmwareSettings::SerialPortName.size()),
                        FirmwareSettings::SerialPortName.data(),
                        FirmwareSettings::SERIAL_BAUD_RATE,
                        static_cast<int>(FirmwareSettings::DEVICE_NAME.size()),
                        FirmwareSettings::DEVICE_NAME.data(),
                        static_cast<int>(AppSettings::AppVersionString.size()),
                        AppSettings::AppVersionString.data());
    queueLog(Messages::LogLevelType::DEBUG, startedDebugMessage.data());
}

void loop()
{
    // delete the loop task of the framework
    vTaskDelete(nullptr);
}

void queueLog(const sercom::Messages::LogLevelType logLevelType, const std::string_view text)
{
    using namespace sercom;

    // copy the text into the log line
    LogLine::LogLine logLine;
    logLine.logLevelType = logLevelType;
    logLine.textLength = text.copy(logLine.text.data(), logLine.text.size());

    // queue the log line or drop it
    [[maybe_unused]] const BaseType_t queued
        = xQueueSend(logQueue,
                     &logLine,
                     pdMS_TO_TICKS(FirmwareSettings::LOG_QUEUE_TIMEOUT_MSEC.count()));
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

        // send the queued log lines to the host
        LogLine::LogLine logLine;
        while (xQueueReceive(logQueue,
                             &logLine,
                             pdMS_TO_TICKS(FirmwareSettings::LOG_QUEUE_TIMEOUT_MSEC.count()))
               == pdTRUE) {
            const std::string_view logText(logLine.text.data(), logLine.textLength);
            [[maybe_unused]] const bool sent
                = deviceApplication.sendLogReportResponse(logLine.logLevelType, logText);
        }

        // maintain the device application
        deviceApplication.maintain();
    }
}

void heartbeatTask([[maybe_unused]] void *parameter)
{
    using namespace sercom;

    // report the health of the device periodically
    while (true) {
        // wait for the next heartbeat
        vTaskDelay(pdMS_TO_TICKS(FirmwareSettings::HEARTBEAT_PERIOD_MSEC.count()));

        // notify the user
        std::array<char, FirmwareSettings::LOG_LINE_SIZE> debugMessage
            = std::array<char, FirmwareSettings::LOG_LINE_SIZE>();
        [[maybe_unused]] const int debugMessageLength
            = std::snprintf(debugMessage.data(),
                            debugMessage.size(),
                            "Heartbeat: free heap %" PRIu32
                            " bytes, protocol stack headroom %" PRIu32 " bytes",
                            esp32Platform.getFreeHeapSize(),
                            static_cast<uint32_t>(uxTaskGetStackHighWaterMark(protocolTaskHandle)));
        queueLog(Messages::LogLevelType::DEBUG, debugMessage.data());
    }
}

#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <string_view>

#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/stream_buffer.h>

#include "../../../app/include/Messages.h"
#include "../../../core/include/Router.h"

/*
 * Set up the device and start the receive, protocol, console and heartbeat tasks.
 * A router also opens its secondary serial port and starts the receive task of it.
 */
void setup();

/*
 * Delete the loop task of the framework because every job runs in its own task.
 */
void loop();

/*
 * Queue a log line for the host from any task and drop it when the log queue stays full.
 */
void queueLog(sercom::Messages::LogLevelType logLevelType, std::string_view text);

/*
 * Get the router configuration with the node address and the proxy address of the build.
 */
[[nodiscard]] sercom::Router::RouterConfiguration getRouterConfiguration();

/*
 * Move the bytes of the UART driver into the receive stream buffer as soon as they arrive.
 */
[[noreturn]] void uartReceiveTask(void *parameter);

/*
 * Move the bytes of the UART driver of the secondary serial port of a router into its own stream buffer.
 */
[[noreturn]] void secondaryReceiveTask(void *parameter);

/*
 * Move the bytes of a UART driver into a stream buffer as soon as they arrive, for ever.
 */
[[noreturn]] void receiveSerialData(HardwareSerial &serial, StreamBufferHandle_t streamBuffer);

/*
 * Feed the received bytes to the firmware role, report the queued log lines through it and maintain it.
 * The firmware role traces the frames on a printed console and blinks the LED after a valid frame.
 * A router also feeds the waiting bytes of its secondary serial port and publishes its router statistics.
 */
[[noreturn]] void protocolTask(void *parameter);

/*
 * Write the lines of the console buffer to the serial port of the console.
 */
[[noreturn]] void consoleTask(void *parameter);

/*
 * Report the free heap and the stack headroom of the protocol task periodically.
 * A router also reports its forwarded, expired and unreachable frames.
 */
[[noreturn]] void heartbeatTask(void *parameter);

#endif // SERCOM_MAIN_H

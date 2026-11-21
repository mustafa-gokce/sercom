#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <string_view>

#include "../../../app/include/Messages.h"

/*
 * Set up the device and start the receive, protocol, console and heartbeat tasks.
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
 * Move the bytes of the UART driver into the receive stream buffer as soon as they arrive.
 */
[[noreturn]] void uartReceiveTask(void *parameter);

/*
 * Feed the received bytes to the firmware role, report the queued log lines through it and maintain it.
 * The firmware role traces the frames on a printed console and blinks the LED after a valid frame.
 */
[[noreturn]] void protocolTask(void *parameter);

/*
 * Write the lines of the console buffer to the serial port of the console.
 */
[[noreturn]] void consoleTask(void *parameter);

/*
 * Report the free heap and the stack headroom of the protocol task periodically.
 */
[[noreturn]] void heartbeatTask(void *parameter);

#endif // SERCOM_MAIN_H

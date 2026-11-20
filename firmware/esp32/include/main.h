#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <string_view>

#include "../../../app/include/Messages.h"

/*
 * Set up the device and start the receive, protocol and heartbeat tasks.
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
 * Feed the received bytes and the queued log lines to the device application and maintain it.
 */
[[noreturn]] void protocolTask(void *parameter);

/*
 * Report the free heap periodically.
 */
[[noreturn]] void heartbeatTask(void *parameter);

#endif // SERCOM_MAIN_H

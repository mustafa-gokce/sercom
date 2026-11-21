#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

/*
 * Set up the device and start the receive and protocol tasks.
 */
void setup();

/*
 * Delete the loop task of the framework because every job runs in its own task.
 */
void loop();

/*
 * Move the bytes of the UART driver into the receive stream buffer as soon as they arrive.
 */
[[noreturn]] void uartReceiveTask(void *parameter);

/*
 * Feed the received bytes to the device application and maintain it.
 */
[[noreturn]] void protocolTask(void *parameter);

#endif // SERCOM_MAIN_H

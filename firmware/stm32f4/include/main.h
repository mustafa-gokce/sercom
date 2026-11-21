#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <cstddef>

/*
 * Configure the board, open the serial ports and run the firmware role in a superloop.
 */
int main();

/*
 * Print the boot banner with the name and the version of the device and its serial ports.
 */
void printBanner();

/*
 * Write a formatted line of the boot banner to the console serial port.
 */
void writeConsoleLine(const char *line, int lineLength);

/*
 * Report the free memory and the dropped received bytes through the firmware role.
 */
void reportHeartbeat();

extern "C" {

/*
 * Count one millisecond of the SysTick timer.
 */
void SysTick_Handler();

/*
 * Serve the receive and transmit interrupts of USART1.
 */
void USART1_IRQHandler();

/*
 * Serve the receive and transmit interrupts of USART2.
 */
void USART2_IRQHandler();

/*
 * Refuse every heap request of the C library, because the firmware has no heap.
 */
void *_sbrk(ptrdiff_t increment);
}

#endif // SERCOM_MAIN_H

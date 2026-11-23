#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <cstddef>

#include <sys/types.h>

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

/*
 * Refuse every close request of the C library, because the firmware has no files.
 */
int _close(int file);

/*
 * Refuse every seek request of the C library, because the firmware has no files.
 */
off_t _lseek(int file, off_t offset, int origin);

/*
 * Refuse every read request of the C library, because the firmware has no files.
 */
ssize_t _read(int file, void *buffer, size_t size);

/*
 * Refuse every write request of the C library, because the firmware has no files.
 */
ssize_t _write(int file, const void *buffer, size_t size);
}

#endif // SERCOM_MAIN_H

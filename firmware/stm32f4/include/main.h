#ifndef SERCOM_MAIN_H
#define SERCOM_MAIN_H

#include <cstddef>

#include <sys/types.h>

/*
 * Configure the board, start the millisecond clock and sleep in a superloop.
 */
int main();

extern "C" {

/*
 * Count one millisecond of the SysTick timer.
 */
void SysTick_Handler();

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

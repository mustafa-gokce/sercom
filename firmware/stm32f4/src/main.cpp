#include "../include/main.h"

#include <cerrno>
#include <cstdint>

#include <stm32f4xx.h>

#include "../include/FirmwareSettings.h"
#include "../include/Stm32Clock.h"
#include "../include/Stm32Platform.h"

/*
 * Global variables.
 */
static sercom::Stm32Clock::Stm32Clock stm32Clock; // millisecond clock of the SysTick timer

// initialize device
static sercom::Stm32Platform::Stm32Platform stm32Platform(stm32Clock,
                                                          sercom::FirmwareSettings::DEVICE_NAME);

int main()
{
    // configure the board and paint the free memory
    stm32Platform.begin();

    // start the millisecond clock
    stm32Clock.begin();

    // run the superloop
    while (true) {
        // sleep until the next interrupt, the SysTick timer wakes the loop every millisecond
        __WFI();
    }
}

void SysTick_Handler()
{
    // count one millisecond
    stm32Clock.processTick();
}

void *_sbrk([[maybe_unused]] const ptrdiff_t increment)
{
    // NOTE: newlib links malloc for snprintf, every request of it fails here
    static constexpr intptr_t failedBreak = -1;
    errno = ENOMEM;
    return reinterpret_cast<void *>(failedBreak);
}

int _close([[maybe_unused]] const int file)
{
    // NOTE: the exit of newlib links the stdio streams, every close request of them fails here
    static constexpr int failedClose = -1;
    errno = EBADF;
    return failedClose;
}

off_t _lseek([[maybe_unused]] const int file,
             [[maybe_unused]] const off_t offset,
             [[maybe_unused]] const int origin)
{
    // NOTE: the exit of newlib links the stdio streams, every seek request of them fails here
    static constexpr off_t failedSeek = -1;
    errno = EBADF;
    return failedSeek;
}

ssize_t _read([[maybe_unused]] const int file,
              [[maybe_unused]] void *buffer,
              [[maybe_unused]] const size_t size)
{
    // NOTE: the exit of newlib links the stdio streams, every read request of them fails here
    static constexpr ssize_t failedRead = -1;
    errno = EBADF;
    return failedRead;
}

ssize_t _write([[maybe_unused]] const int file,
               [[maybe_unused]] const void *buffer,
               [[maybe_unused]] const size_t size)
{
    // NOTE: the exit of newlib links the stdio streams, every write request of them fails here
    static constexpr ssize_t failedWrite = -1;
    errno = EBADF;
    return failedWrite;
}

#ifndef SERCOM_STM32_CLOCK_H
#define SERCOM_STM32_CLOCK_H

#include <chrono>
#include <cstdint>

#include "../../../core/include/Clock.h"

namespace sercom::Stm32Clock {

class Stm32Clock final : public Clock::Clock
{
public:
    /*
     * Clock that counts the milliseconds of the SysTick timer in 64 bits.
     */
    Stm32Clock();
    ~Stm32Clock() final;

    // delete copy constructor and copy assignment operator
    Stm32Clock(const Stm32Clock &) = delete;
    Stm32Clock &operator=(const Stm32Clock &) = delete;

    // delete move constructor and move assignment operator
    Stm32Clock(Stm32Clock &&) = delete;
    Stm32Clock &operator=(Stm32Clock &&) = delete;

public:
    /*
     * Start the SysTick timer from zero with one interrupt per millisecond of the system clock.
     */
    void begin();

    /*
     * Count one millisecond, called by the SysTick interrupt handler.
     */
    void processTick();

public:
    /*
     * Get the time since the start of the SysTick timer in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final;

private:
    volatile uint64_t tickCount; // milliseconds since the start of the SysTick timer
};

} // namespace sercom::Stm32Clock

#endif // SERCOM_STM32_CLOCK_H

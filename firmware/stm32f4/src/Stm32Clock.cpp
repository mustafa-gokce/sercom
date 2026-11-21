#include "../include/Stm32Clock.h"

#include <stm32f4xx.h>

#include "../include/FirmwareSettings.h"

namespace sercom::Stm32Clock {

Stm32Clock::Stm32Clock()
    : tickCount(0)
{}

Stm32Clock::~Stm32Clock() {}

void Stm32Clock::begin()
{
    // start the SysTick timer on the system clock with its interrupt from zero
    this->tickCount = 0;
    [[maybe_unused]] const uint32_t sysTickFailed = SysTick_Config(
        FirmwareSettings::SystemClockFrequency / FirmwareSettings::SYSTICK_FREQUENCY);
}

void Stm32Clock::processTick()
{
    // count one millisecond
    ++this->tickCount;
}

std::chrono::milliseconds Stm32Clock::getTime() const
{
    // NOTE: a 64 bit count takes two loads, so the SysTick interrupt must wait for both
    const uint32_t interruptMask = __get_PRIMASK();
    __disable_irq();
    const uint64_t currentTickCount = this->tickCount;
    __set_PRIMASK(interruptMask);

    // expose the milliseconds of the SysTick timer
    return std::chrono::milliseconds(static_cast<std::chrono::milliseconds::rep>(currentTickCount));
}

} // namespace sercom::Stm32Clock

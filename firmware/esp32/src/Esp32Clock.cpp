#include "../include/Esp32Clock.h"

#include <esp_timer.h>

namespace sercom::Esp32Clock {

Esp32Clock::~Esp32Clock() {}

std::chrono::milliseconds Esp32Clock::getTime() const
{
    // expose the microsecond timer in milliseconds
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::microseconds(esp_timer_get_time()));
}

} // namespace sercom::Esp32Clock

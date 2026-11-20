#ifndef SERCOM_ESP32_CLOCK_H
#define SERCOM_ESP32_CLOCK_H

#include <chrono>

#include "../../../core/include/Clock.h"

namespace sercom::Esp32Clock {

class Esp32Clock final : public Clock::Clock
{
public:
    /*
     * Clock based on the 64 bit microsecond timer of the ESP-IDF.
     */
    Esp32Clock() = default;
    ~Esp32Clock() final;

    // delete copy constructor and copy assignment operator
    Esp32Clock(const Esp32Clock &) = delete;
    Esp32Clock &operator=(const Esp32Clock &) = delete;

    // delete move constructor and move assignment operator
    Esp32Clock(Esp32Clock &&) = delete;
    Esp32Clock &operator=(Esp32Clock &&) = delete;

public:
    /*
     * Get the time since boot in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final;
};

} // namespace sercom::Esp32Clock

#endif // SERCOM_ESP32_CLOCK_H

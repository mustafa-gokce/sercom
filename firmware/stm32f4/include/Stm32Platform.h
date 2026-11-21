#ifndef SERCOM_STM32_PLATFORM_H
#define SERCOM_STM32_PLATFORM_H

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "../../../core/include/Clock.h"
#include "../../common/include/FirmwarePlatform.h"

namespace sercom::Stm32Platform {

class Stm32Platform final : public FirmwarePlatform::FirmwarePlatform
{
public:
    /*
     * STM32F4 hardware with the user LED, the free memory below the stack and the temperature sensor.
     * The firmware has no heap, so the free heap size is the memory the stack never reached.
     */
    explicit Stm32Platform(const Clock::Clock &clock, std::string_view deviceName);
    ~Stm32Platform() final;

    // delete copy constructor and copy assignment operator
    Stm32Platform(const Stm32Platform &) = delete;
    Stm32Platform &operator=(const Stm32Platform &) = delete;

    // delete move constructor and move assignment operator
    Stm32Platform(Stm32Platform &&) = delete;
    Stm32Platform &operator=(Stm32Platform &&) = delete;

public:
    /*
     * Enable the clocks, configure the LED pin and the temperature sensor and paint the free memory.
     */
    void begin();

public:
    /*
     * Get whether the LED is on.
     */
    [[nodiscard]] bool getLedOn() const final;

    /*
     * Set whether the LED is on.
     */
    void setLedOn(bool ledOn) final;

    /*
     * Get the free memory in bytes between the static data and the deepest stack use so far.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const final;

    /*
     * Get the temperature of the internal sensor with its factory calibration in 0.01 degree Celsius steps.
     * A conversion that does not end within the conversion timeout leaves the temperature unavailable.
     */
    [[nodiscard]] int16_t getTemperature() const final;

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final;

    /*
     * Show the LED lit or dark without changing whether the LED is on.
     */
    void showLed(bool ledLit) final;

private:
    const Clock::Clock &clock;         // reference for clock
    const std::string_view deviceName; // reported device name
    bool ledOn;                        // LED is on
    size_t paintedWordCount;           // words of free memory painted at the start
};

} // namespace sercom::Stm32Platform

#endif // SERCOM_STM32_PLATFORM_H

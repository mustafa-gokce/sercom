#ifndef SERCOM_ESP32_PLATFORM_H
#define SERCOM_ESP32_PLATFORM_H

#include <cstdint>
#include <string_view>

#include "../../../app/include/DeviceApplication.h"

namespace sercom::Esp32Platform {

class Esp32Platform final : public DeviceApplication::DevicePlatform
{
public:
    /*
     * ESP32 hardware with the on board LED, heap statistics and the internal temperature sensor.
     */
    explicit Esp32Platform(uint8_t ledPin, std::string_view deviceName);
    ~Esp32Platform() final;

    // delete copy constructor and copy assignment operator
    Esp32Platform(const Esp32Platform &) = delete;
    Esp32Platform &operator=(const Esp32Platform &) = delete;

    // delete move constructor and move assignment operator
    Esp32Platform(Esp32Platform &&) = delete;
    Esp32Platform &operator=(Esp32Platform &&) = delete;

public:
    /*
     * Configure the LED pin and switch the LED off.
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
     * Get the free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const final;

    /*
     * Get the temperature of the internal sensor in 0.01 degree Celsius steps.
     */
    [[nodiscard]] int16_t getTemperature() const final;

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final;

private:
    const uint8_t ledPin;              // GPIO of the LED
    const std::string_view deviceName; // reported device name
    bool ledOn;                        // LED is on
};

} // namespace sercom::Esp32Platform

#endif // SERCOM_ESP32_PLATFORM_H

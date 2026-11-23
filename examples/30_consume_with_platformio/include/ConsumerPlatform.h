#ifndef SERCOM_CONSUMER_PLATFORM_H
#define SERCOM_CONSUMER_PLATFORM_H

#include <chrono>
#include <cstdint>
#include <string_view>

#include "../../../core/include/Clock.h"
#include "../../../firmware/common/include/FirmwarePlatform.h"

namespace sercom::ConsumerPlatform {

constexpr uint8_t LED_PIN = 2;                             // GPIO of the on board LED
constexpr std::string_view DEVICE_NAME = "consumer-esp32"; // reported device name

class ConsumerPlatform final : public FirmwarePlatform::FirmwarePlatform, public Clock::Clock
{
public:
    /*
     * Board of the consumer firmware with the on board LED, the heap statistics and the clock of the framework.
     * It reads no temperature sensor and reports the temperature as unavailable.
     */
    explicit ConsumerPlatform(uint8_t ledPin, std::string_view deviceName);
    ~ConsumerPlatform() final;

    // delete copy constructor and copy assignment operator
    ConsumerPlatform(const ConsumerPlatform &) = delete;
    ConsumerPlatform &operator=(const ConsumerPlatform &) = delete;

    // delete move constructor and move assignment operator
    ConsumerPlatform(ConsumerPlatform &&) = delete;
    ConsumerPlatform &operator=(ConsumerPlatform &&) = delete;

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
     * Get the temperature in 0.01 degree Celsius steps, unavailable without a sensor.
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

    /*
     * Get the time since boot in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final;

private:
    const uint8_t ledPin;              // GPIO of the LED
    const std::string_view deviceName; // reported device name
    bool ledOn;                        // LED is on
};

} // namespace sercom::ConsumerPlatform

#endif // SERCOM_CONSUMER_PLATFORM_H

#ifndef SERCOM_SIMULATED_PLATFORM_H
#define SERCOM_SIMULATED_PLATFORM_H

#include <cstdint>
#include <string>
#include <string_view>

#include "../../app/include/DeviceApplication.h"
#include "../../core/include/Clock.h"

namespace sercom::SimulatedPlatform {

class SimulatedPlatform final : public DeviceApplication::DevicePlatform
{
public:
    /*
     * Device hardware simulated in memory, so the protocol can be studied without a board.
     */
    explicit SimulatedPlatform(const Clock::Clock &clock, const std::string &deviceName);
    ~SimulatedPlatform() final;

    // delete copy constructor and copy assignment operator
    SimulatedPlatform(const SimulatedPlatform &) = delete;
    SimulatedPlatform &operator=(const SimulatedPlatform &) = delete;

    // delete move constructor and move assignment operator
    SimulatedPlatform(SimulatedPlatform &&) = delete;
    SimulatedPlatform &operator=(SimulatedPlatform &&) = delete;

public:
    /*
     * Get whether the simulated LED is on.
     */
    [[nodiscard]] bool getLedOn() const final;

    /*
     * Set whether the simulated LED is on and report the change.
     */
    void setLedOn(bool ledOn) final;

    /*
     * Get the simulated free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const final;

    /*
     * Get a temperature that follows a sine wave around the simulated temperature.
     */
    [[nodiscard]] int16_t getTemperature() const final;

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final;

private:
    const Clock::Clock &clock;    // reference for clock
    const std::string deviceName; // device name of the device info response
    bool ledOn;                   // simulated LED is on
};

} // namespace sercom::SimulatedPlatform

#endif // SERCOM_SIMULATED_PLATFORM_H

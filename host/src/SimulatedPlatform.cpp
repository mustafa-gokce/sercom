#include "../include/SimulatedPlatform.h"

#include <cmath>

#include "../include/HostSettings.h"
#include "../include/HostUtils.h"

namespace sercom::SimulatedPlatform {

SimulatedPlatform::SimulatedPlatform(const Clock::Clock &clock, const std::string &deviceName)
    : clock(clock)
    , deviceName(deviceName)
    , ledOn(false)
{}

SimulatedPlatform::~SimulatedPlatform() {}

bool SimulatedPlatform::getLedOn() const
{
    // expose whether the simulated LED is on
    return this->ledOn;
}

void SimulatedPlatform::setLedOn(const bool ledOn)
{
    // set the simulated LED
    this->ledOn = ledOn;

    // notify the user
    const std::string debugMessage = std::string("LED switched ") + (ledOn ? "on" : "off");
    HostUtils::logDebug(debugMessage);
}

uint32_t SimulatedPlatform::getFreeHeapSize() const
{
    // expose the simulated free heap size
    return HostSettings::SIMULATED_FREE_HEAP_SIZE;
}

int16_t SimulatedPlatform::getTemperature() const
{
    // get the phase of the temperature period
    const double elapsedPeriodCount = static_cast<double>(this->clock.getTime().count())
                                      / static_cast<double>(
                                          HostSettings::SIMULATED_TEMPERATURE_PERIOD_MSEC.count());
    const double phase = 2.0 * HostSettings::PI * elapsedPeriodCount;

    // expose the temperature on the sine wave
    const double temperature = HostSettings::SIMULATED_TEMPERATURE
                               + HostSettings::SIMULATED_TEMPERATURE_AMPLITUDE * std::sin(phase);
    return static_cast<int16_t>(std::lround(temperature));
}

std::string_view SimulatedPlatform::getDeviceName() const
{
    // expose the device name
    return this->deviceName;
}

} // namespace sercom::SimulatedPlatform

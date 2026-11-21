#include "../include/Esp32Platform.h"

#include <cmath>

#include <Arduino.h>

namespace sercom::Esp32Platform {

Esp32Platform::Esp32Platform(const uint8_t ledPin, const std::string_view deviceName)
    : ledPin(ledPin)
    , deviceName(deviceName)
    , ledOn(false)
{}

Esp32Platform::~Esp32Platform() {}

void Esp32Platform::begin()
{
    // configure the LED pin and switch the LED off
    pinMode(this->ledPin, OUTPUT);
    digitalWrite(this->ledPin, LOW);
}

bool Esp32Platform::getLedOn() const
{
    // expose whether the LED is on
    return this->ledOn;
}

void Esp32Platform::setLedOn(const bool ledOn)
{
    // switch the LED and keep its state
    digitalWrite(this->ledPin, ledOn ? HIGH : LOW);
    this->ledOn = ledOn;
}

uint32_t Esp32Platform::getFreeHeapSize() const
{
    // expose the free heap size
    return ESP.getFreeHeap();
}

int16_t Esp32Platform::getTemperature() const
{
    // read the uncalibrated internal sensor in degree Celsius
    const float temperature = temperatureRead();

    // expose the temperature in 0.01 degree Celsius steps
    return static_cast<int16_t>(std::lround(static_cast<double>(temperature) * 100.0));
}

std::string_view Esp32Platform::getDeviceName() const
{
    // expose the device name
    return this->deviceName;
}

} // namespace sercom::Esp32Platform

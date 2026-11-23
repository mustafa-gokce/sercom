#include "../include/ConsumerPlatform.h"

#include <Arduino.h>

#include "../../../app/include/Messages.h"

namespace sercom::ConsumerPlatform {

ConsumerPlatform::ConsumerPlatform(const uint8_t ledPin, const std::string_view deviceName)
    : ledPin(ledPin)
    , deviceName(deviceName)
    , ledOn(false)
{}

ConsumerPlatform::~ConsumerPlatform() {}

void ConsumerPlatform::begin()
{
    // configure the LED pin and switch the LED off
    pinMode(this->ledPin, OUTPUT);
    digitalWrite(this->ledPin, LOW);
}

bool ConsumerPlatform::getLedOn() const
{
    // expose whether the LED is on
    return this->ledOn;
}

void ConsumerPlatform::setLedOn(const bool ledOn)
{
    // switch the LED and keep its state
    digitalWrite(this->ledPin, ledOn ? HIGH : LOW);
    this->ledOn = ledOn;
}

uint32_t ConsumerPlatform::getFreeHeapSize() const
{
    // expose the free heap size
    return ESP.getFreeHeap();
}

int16_t ConsumerPlatform::getTemperature() const
{
    // expose the unavailable temperature of a board without a sensor
    return Messages::UNAVAILABLE_TEMPERATURE;
}

std::string_view ConsumerPlatform::getDeviceName() const
{
    // expose the device name
    return this->deviceName;
}

void ConsumerPlatform::showLed(const bool ledLit)
{
    // switch the LED without changing its state
    digitalWrite(this->ledPin, ledLit ? HIGH : LOW);
}

std::chrono::milliseconds ConsumerPlatform::getTime() const
{
    // expose the milliseconds since boot of the framework
    return std::chrono::milliseconds(millis());
}

} // namespace sercom::ConsumerPlatform

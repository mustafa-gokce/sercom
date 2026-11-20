#include "../include/ActivityLed.h"

#include <Arduino.h>

#include "../include/FirmwareSettings.h"

namespace sercom::ActivityLed {

ActivityLed::ActivityLed(const uint8_t ledPin,
                         const DeviceApplication::DevicePlatform &devicePlatform,
                         const Clock::Clock &clock)
    : ledPin(ledPin)
    , devicePlatform(devicePlatform)
    , clock(clock)
    , blinking(false)
    , lastBlinkTimestamp(0)
{}

ActivityLed::~ActivityLed() {}

void ActivityLed::startBlink()
{
    // activity blink period must have passed since the last activity blink
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (this->blinking
        || currentTime - this->lastBlinkTimestamp < FirmwareSettings::ACTIVITY_BLINK_PERIOD_MSEC) {
        return;
    }

    // show the inverted LED state
    digitalWrite(this->ledPin, this->devicePlatform.getLedOn() ? LOW : HIGH);
    this->blinking = true;
    this->lastBlinkTimestamp = currentTime;
}

void ActivityLed::processActivityLed()
{
    // activity blink must be shown
    if (!this->blinking) {
        return;
    }

    // activity blink must be over
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    if (currentTime - this->lastBlinkTimestamp < FirmwareSettings::ACTIVITY_BLINK_DURATION_MSEC) {
        return;
    }

    // restore the LED state of the platform
    digitalWrite(this->ledPin, this->devicePlatform.getLedOn() ? HIGH : LOW);
    this->blinking = false;
}

} // namespace sercom::ActivityLed

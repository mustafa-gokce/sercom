#include "../include/ActivityLed.h"

#include "../include/CommonSettings.h"

namespace sercom::ActivityLed {

ActivityLed::ActivityLed(FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
                         const Clock::Clock &clock)
    : firmwarePlatform(firmwarePlatform)
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
        || currentTime - this->lastBlinkTimestamp < CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC) {
        return;
    }

    // show the inverted LED state
    this->firmwarePlatform.showLed(!this->firmwarePlatform.getLedOn());
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
    if (currentTime - this->lastBlinkTimestamp < CommonSettings::ACTIVITY_BLINK_DURATION_MSEC) {
        return;
    }

    // restore the LED state of the platform
    this->firmwarePlatform.showLed(this->firmwarePlatform.getLedOn());
    this->blinking = false;
}

} // namespace sercom::ActivityLed

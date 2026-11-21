#ifndef SERCOM_ACTIVITY_LED_H
#define SERCOM_ACTIVITY_LED_H

#include <chrono>
#include <cstdint>

#include "../../../app/include/DeviceApplication.h"
#include "../../../core/include/Clock.h"

namespace sercom::ActivityLed {

class ActivityLed final
{
public:
    /*
     * Activity blink that inverts the LED for a moment and then restores the platform LED state.
     * The LED state stays the one that the last set LED request asked for.
     */
    explicit ActivityLed(uint8_t ledPin,
                         const DeviceApplication::DevicePlatform &devicePlatform,
                         const Clock::Clock &clock);
    ~ActivityLed();

    // delete copy constructor and copy assignment operator
    ActivityLed(const ActivityLed &) = delete;
    ActivityLed &operator=(const ActivityLed &) = delete;

    // delete move constructor and move assignment operator
    ActivityLed(ActivityLed &&) = delete;
    ActivityLed &operator=(ActivityLed &&) = delete;

public:
    /*
     * Start an activity blink unless one started within the activity blink period.
     */
    void startBlink();

    /*
     * Restore the LED state of the platform when the activity blink is over.
     */
    void processActivityLed();

private:
    const uint8_t ledPin;                                    // GPIO of the LED
    const DeviceApplication::DevicePlatform &devicePlatform; // reference for device platform
    const Clock::Clock &clock;                               // reference for clock
    bool blinking;                                           // LED shows an activity blink
    std::chrono::milliseconds lastBlinkTimestamp;            // start time of the latest blink
};

} // namespace sercom::ActivityLed

#endif // SERCOM_ACTIVITY_LED_H

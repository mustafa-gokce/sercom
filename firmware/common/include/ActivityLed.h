#ifndef SERCOM_ACTIVITY_LED_H
#define SERCOM_ACTIVITY_LED_H

#include <chrono>

#include "../../../core/include/Clock.h"

#include "../include/FirmwarePlatform.h"

namespace sercom::ActivityLed {

class ActivityLed final
{
public:
    /*
     * Activity blink that inverts the LED for a moment and then restores the platform LED state.
     * The LED state stays the one that the last set LED request asked for.
     */
    explicit ActivityLed(FirmwarePlatform::FirmwarePlatform &firmwarePlatform,
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
    FirmwarePlatform::FirmwarePlatform &firmwarePlatform; // reference for firmware platform
    const Clock::Clock &clock;                            // reference for clock
    bool blinking;                                        // LED shows an activity blink
    std::chrono::milliseconds lastBlinkTimestamp;         // start time of the latest blink
};

} // namespace sercom::ActivityLed

#endif // SERCOM_ACTIVITY_LED_H

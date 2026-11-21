#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include <catch2/catch_all.hpp>

#include "../core/include/Clock.h"
#include "../firmware/common/include/ActivityLed.h"
#include "../firmware/common/include/CommonSettings.h"
#include "../firmware/common/include/FirmwarePlatform.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

class MockClock final : public Clock::Clock
{
public:
    /*
     * Mock clock that only moves when the test moves it.
     */
    MockClock()
        : currentTime(0)
    {}
    ~MockClock() final {}

    // delete copy constructor and copy assignment operator
    MockClock(const MockClock &) = delete;
    MockClock &operator=(const MockClock &) = delete;

    // delete move constructor and move assignment operator
    MockClock(MockClock &&) = delete;
    MockClock &operator=(MockClock &&) = delete;

public:
    /*
     * Get the current time in milliseconds.
     */
    [[nodiscard]] std::chrono::milliseconds getTime() const final
    {
        // expose the current time
        return this->currentTime;
    }

    /*
     * Move the current time forward.
     */
    void advanceTime(const std::chrono::milliseconds duration)
    {
        // move the current time forward
        this->currentTime += duration;
    }

private:
    std::chrono::milliseconds currentTime; // current time in milliseconds
};

class MockFirmwarePlatform final : public FirmwarePlatform::FirmwarePlatform
{
public:
    /*
     * Mock firmware platform with an LED that remembers whether it is lit.
     */
    MockFirmwarePlatform()
        : ledOn(false)
        , ledLit(false)
        , shownLedCount(0)
    {}
    ~MockFirmwarePlatform() final {}

    // delete copy constructor and copy assignment operator
    MockFirmwarePlatform(const MockFirmwarePlatform &) = delete;
    MockFirmwarePlatform &operator=(const MockFirmwarePlatform &) = delete;

    // delete move constructor and move assignment operator
    MockFirmwarePlatform(MockFirmwarePlatform &&) = delete;
    MockFirmwarePlatform &operator=(MockFirmwarePlatform &&) = delete;

public:
    /*
     * Get whether the LED is on.
     */
    [[nodiscard]] bool getLedOn() const final
    {
        // expose whether the LED is on
        return this->ledOn;
    }

    /*
     * Set whether the LED is on.
     */
    void setLedOn(const bool ledOn) final
    {
        // set whether the LED is on and show it
        this->ledOn = ledOn;
        this->ledLit = ledOn;
    }

    /*
     * Get the free heap size in bytes.
     */
    [[nodiscard]] uint32_t getFreeHeapSize() const final
    {
        // expose no free heap
        return 0;
    }

    /*
     * Get the temperature in 0.01 degree Celsius steps.
     */
    [[nodiscard]] int16_t getTemperature() const final
    {
        // expose zero degrees
        return 0;
    }

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final
    {
        // expose a fixed device name
        return {"unit-under-test"};
    }

    /*
     * Show the LED lit or dark without changing whether the LED is on.
     */
    void showLed(const bool ledLit) final
    {
        // show the LED
        this->ledLit = ledLit;

        // keep track of shown LED states
        ++this->shownLedCount;
    }

public:
    /*
     * Get whether the LED is lit.
     */
    [[nodiscard]] bool getLedLit() const
    {
        // expose whether the LED is lit
        return this->ledLit;
    }

    /*
     * Get the number of shown LED states.
     */
    [[nodiscard]] size_t getShownLedCount() const
    {
        // expose the number of shown LED states
        return this->shownLedCount;
    }

private:
    bool ledOn;           // LED is on
    bool ledLit;          // LED is lit
    size_t shownLedCount; // number of shown LED states
};

} // namespace

class TestActivityLed
{
public:
    /*
     * Activity LED test fixture.
     */
    TestActivityLed()
    {
        // initialize activity LED
        this->activityLed.reset(new ActivityLed::ActivityLed(this->firmwarePlatform, this->clock));
    }
    ~TestActivityLed()
    {
        // close activity LED
        this->activityLed.reset(nullptr);
    }

    // delete copy and move operators
    TestActivityLed(const TestActivityLed &other) = delete;
    TestActivityLed &operator=(const TestActivityLed &other) = delete;
    TestActivityLed(TestActivityLed &&other) noexcept = delete;
    TestActivityLed &operator=(TestActivityLed &&other) noexcept = delete;

protected:
    MockClock clock;                                       // clock of the activity LED
    MockFirmwarePlatform firmwarePlatform;                 // hardware with the LED
    std::unique_ptr<ActivityLed::ActivityLed> activityLed; // activity LED under test
};

TEST_CASE_METHOD(TestActivityLed,
                 "An activity blink inverts the LED and restores its state afterwards",
                 "[ActivityLed][testBlink]")
{
    // start an activity blink of a dark LED
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC);
    activityLed->startBlink();
    REQUIRE(firmwarePlatform.getLedLit());

    // check that the blink lasts for its duration
    activityLed->processActivityLed();
    REQUIRE(firmwarePlatform.getLedLit());
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_DURATION_MSEC);
    activityLed->processActivityLed();
    REQUIRE_FALSE(firmwarePlatform.getLedLit());

    // start an activity blink of an LED that is on
    firmwarePlatform.setLedOn(true);
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC);
    activityLed->startBlink();
    REQUIRE_FALSE(firmwarePlatform.getLedLit());
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_DURATION_MSEC);
    activityLed->processActivityLed();
    REQUIRE(firmwarePlatform.getLedLit());
    REQUIRE(firmwarePlatform.getLedOn());
}

TEST_CASE_METHOD(TestActivityLed,
                 "A second activity blink waits for the activity blink period",
                 "[ActivityLed][testBlinkPeriod]")
{
    // start a first activity blink and let it end
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC);
    activityLed->startBlink();
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_DURATION_MSEC);
    activityLed->processActivityLed();
    REQUIRE(firmwarePlatform.getShownLedCount() == static_cast<size_t>(2));

    // check that a blink within the period is skipped
    activityLed->startBlink();
    REQUIRE(firmwarePlatform.getShownLedCount() == static_cast<size_t>(2));

    // check that a blink after the period is shown
    clock.advanceTime(CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC);
    activityLed->startBlink();
    REQUIRE(firmwarePlatform.getShownLedCount() == static_cast<size_t>(3));
}

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../app/include/AppSettings.h"
#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"
#include "../firmware/common/include/CommonSettings.h"
#include "../firmware/common/include/Console.h"
#include "../firmware/common/include/ConsoleOutput.h"
#include "../firmware/common/include/ControllerRole.h"
#include "../firmware/common/include/DeviceRole.h"
#include "../firmware/common/include/FirmwarePlatform.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

using DataFilter = std::function<bool(size_t sendCount, const std::vector<uint8_t> &data)>;

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
     * Set the current time in milliseconds.
     */
    void setTime(const std::chrono::milliseconds time)
    {
        // set the current time
        this->currentTime = time;
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

class MockTransport final : public Transport::Transport
{
public:
    /*
     * Mock transport that queues the sent data until the test delivers it to the other role.
     */
    MockTransport()
        : sendCount(0)
    {}
    ~MockTransport() final {}

    // delete copy constructor and copy assignment operator
    MockTransport(const MockTransport &) = delete;
    MockTransport &operator=(const MockTransport &) = delete;

    // delete move constructor and move assignment operator
    MockTransport(MockTransport &&) = delete;
    MockTransport &operator=(MockTransport &&) = delete;

public:
    /*
     * Send data into the queue unless the filter drops the data.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, const size_t size) final
    {
        // keep track of sent data
        ++this->sendCount;
        const std::vector<uint8_t> sentData(data, data + size);

        // drop the data the filter rejects
        if (this->filter != nullptr && !this->filter(this->sendCount, sentData)) {
            return true;
        }

        // queue the data
        this->sentDataQueue.push_back(sentData);

        // data sent successfully
        return true;
    }

    /*
     * Receive the oldest data that waits in the queue.
     * Get false when no data waits.
     */
    [[nodiscard]] bool receiveData(std::vector<uint8_t> &data)
    {
        // data must wait in the queue
        if (this->sentDataQueue.empty()) {
            return false;
        }

        // take the oldest data out of the queue
        data = this->sentDataQueue.front();
        this->sentDataQueue.pop_front();

        // data received successfully
        return true;
    }

public:
    /*
     * Get whether no data waits in the queue.
     */
    [[nodiscard]] bool getEmpty() const
    {
        // expose whether the queue is empty
        return this->sentDataQueue.empty();
    }

    /*
     * Set the filter that keeps the data of a send when it returns true.
     */
    void setFilter(const DataFilter &filter)
    {
        // set the filter
        this->filter = filter;
    }

private:
    std::deque<std::vector<uint8_t>> sentDataQueue; // sent data that waits for the other role
    DataFilter filter;                              // filter that keeps or drops sent data
    size_t sendCount;                               // number of sends so far
};

class MockFirmwarePlatform final : public FirmwarePlatform::FirmwarePlatform
{
public:
    /*
     * Mock firmware platform with a fixed device and an LED that counts how often it was shown.
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
        // expose a fixed free heap size
        static constexpr uint32_t freeHeapSize = 4242;
        return freeHeapSize;
    }

    /*
     * Get the temperature in 0.01 degree Celsius steps.
     */
    [[nodiscard]] int16_t getTemperature() const final
    {
        // expose a temperature without a reading
        return Messages::UNAVAILABLE_TEMPERATURE;
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

class MockConsoleOutput final : public ConsoleOutput::ConsoleOutput
{
public:
    /*
     * Mock console output that keeps every written character.
     */
    MockConsoleOutput() = default;
    ~MockConsoleOutput() final {}

    // delete copy constructor and copy assignment operator
    MockConsoleOutput(const MockConsoleOutput &) = delete;
    MockConsoleOutput &operator=(const MockConsoleOutput &) = delete;

    // delete move constructor and move assignment operator
    MockConsoleOutput(MockConsoleOutput &&) = delete;
    MockConsoleOutput &operator=(MockConsoleOutput &&) = delete;

public:
    /*
     * Get the number of characters that the console output takes now.
     */
    [[nodiscard]] size_t getAvailableSize() const final
    {
        // expose room for any line
        return std::numeric_limits<size_t>::max();
    }

    /*
     * Write characters into the kept text.
     */
    void writeText(const char *text, const size_t size) final
    {
        // keep the characters
        this->text.append(text, size);
    }

public:
    /*
     * Get every written character.
     */
    [[nodiscard]] std::string getText() const
    {
        // expose the written text
        return this->text;
    }

private:
    std::string text; // written characters
};

} // namespace

class TestControllerRole
{
public:
    /*
     * Controller role test fixture.
     */
    TestControllerRole()
    {
        // initialize clock
        static constexpr std::chrono::milliseconds startTime = 5000ms;
        this->clock.setTime(startTime);

        // initialize consoles
        this->controllerConsole.reset(
            new Console::Console(this->controllerConsoleOutput, this->clock, true));
        this->deviceConsole.reset(
            new Console::Console(this->deviceConsoleOutput, this->clock, true));

        // initialize controller role and device role
        this->controllerRole.reset(new ControllerRole::ControllerRole(this->controllerTransport,
                                                                      this->clock,
                                                                      this->controllerPlatform,
                                                                      *this->controllerConsole));
        this->deviceRole.reset(new DeviceRole::DeviceRole(this->deviceTransport,
                                                          this->clock,
                                                          this->devicePlatform,
                                                          *this->deviceConsole));
    }
    ~TestControllerRole()
    {
        // close controller role and device role
        this->deviceRole.reset(nullptr);
        this->controllerRole.reset(nullptr);

        // close consoles
        this->deviceConsole.reset(nullptr);
        this->controllerConsole.reset(nullptr);
    }

    /*
     * Deliver the data of the controller role and the device role until no data waits.
     */
    void pump()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->controllerTransport.getEmpty() && this->deviceTransport.getEmpty()) {
                return;
            }

            // deliver the data of the controller to the device role
            std::vector<uint8_t> data;
            while (this->controllerTransport.receiveData(data)) {
                this->deviceRole->onReceivedData(data.data(), data.size());
            }

            // deliver the data of the device to the controller role
            while (this->deviceTransport.receiveData(data)) {
                this->controllerRole->onReceivedData(data.data(), data.size());
            }
        }
    }

    /*
     * Let time pass, maintain both roles and deliver what they sent.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // let time pass
        this->clock.advanceTime(duration);

        // maintain both roles
        this->controllerRole->maintain();
        this->deviceRole->maintain();

        // deliver the sent data
        this->pump();
    }

    /*
     * Run both roles in loop periods until the controller is finished or every step timed out.
     */
    void runUntilFinished()
    {
        // run a bounded number of loop periods
        static constexpr std::chrono::milliseconds loopPeriod = 1ms;
        static constexpr int64_t stepCount = 6;
        static constexpr int64_t maximumLoopCount = stepCount
                                                    * CommonSettings::CONTROLLER_STEP_TIMEOUT_MSEC
                                                    / loopPeriod;
        for (int64_t loop = 0; loop <= maximumLoopCount; ++loop) {
            // controller must not be finished
            if (this->controllerRole->getControllerStepType()
                == ControllerRole::ControllerStepType::FINISHED) {
                return;
            }

            // run one loop period
            this->advance(loopPeriod);
        }
    }

    /*
     * Get whether a text holds a part.
     */
    [[nodiscard]] static bool getTextFound(const std::string &text, const std::string_view part)
    {
        // expose whether the part was found
        return text.find(part) != std::string::npos;
    }

    // delete copy and move operators
    TestControllerRole(const TestControllerRole &other) = delete;
    TestControllerRole &operator=(const TestControllerRole &other) = delete;
    TestControllerRole(TestControllerRole &&other) noexcept = delete;
    TestControllerRole &operator=(TestControllerRole &&other) noexcept = delete;

protected:
    MockClock clock;                                     // clock of both roles
    MockTransport controllerTransport;                   // data sent by the controller
    MockTransport deviceTransport;                       // data sent by the device
    MockFirmwarePlatform controllerPlatform;             // hardware of the controller
    MockFirmwarePlatform devicePlatform;                 // hardware of the device
    MockConsoleOutput controllerConsoleOutput;           // console text of the controller
    MockConsoleOutput deviceConsoleOutput;               // console text of the device
    std::unique_ptr<Console::Console> controllerConsole; // console of the controller
    std::unique_ptr<Console::Console> deviceConsole;     // console of the device
    std::unique_ptr<ControllerRole::ControllerRole> controllerRole; // controller side
    std::unique_ptr<DeviceRole::DeviceRole> deviceRole;             // device side
};

TEST_CASE_METHOD(TestControllerRole,
                 "Controller step type can be converted to string",
                 "[ControllerRole][fromControllerStepType]")
{
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::UNKNOWN)
            == "UNKNOWN");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::CONNECT)
            == "CONNECT");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::PING)
            == "PING");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::INFO)
            == "INFO");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::TELEMETRY)
            == "TELEMETRY");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::STRESS)
            == "STRESS");
    REQUIRE(ControllerRole::fromControllerStepType(ControllerRole::ControllerStepType::FINISHED)
            == "FINISHED");
}

TEST_CASE_METHOD(TestControllerRole,
                 "Controller step type can be parsed from string",
                 "[ControllerRole][toControllerStepType]")
{
    REQUIRE(ControllerRole::toControllerStepType("UNKNOWN")
            == ControllerRole::ControllerStepType::UNKNOWN);
    REQUIRE(ControllerRole::toControllerStepType("CONNECT")
            == ControllerRole::ControllerStepType::CONNECT);
    REQUIRE(ControllerRole::toControllerStepType("PING")
            == ControllerRole::ControllerStepType::PING);
    REQUIRE(ControllerRole::toControllerStepType("INFO")
            == ControllerRole::ControllerStepType::INFO);
    REQUIRE(ControllerRole::toControllerStepType("TELEMETRY")
            == ControllerRole::ControllerStepType::TELEMETRY);
    REQUIRE(ControllerRole::toControllerStepType("STRESS")
            == ControllerRole::ControllerStepType::STRESS);
    REQUIRE(ControllerRole::toControllerStepType("FINISHED")
            == ControllerRole::ControllerStepType::FINISHED);
    REQUIRE(ControllerRole::toControllerStepType("LOREM_IPSUM")
            == ControllerRole::ControllerStepType::UNKNOWN);
    REQUIRE(ControllerRole::toControllerStepType("ping")
            == ControllerRole::ControllerStepType::UNKNOWN);
}

TEST_CASE_METHOD(TestControllerRole,
                 "The controller role passes every step against the device role",
                 "[ControllerRole][testEveryStep]")
{
    // run both roles until the controller is finished
    runUntilFinished();

    // check the result of every step
    const ControllerRole::ControllerReport controllerReport = controllerRole->getControllerReport();
    REQUIRE(controllerRole->getControllerStepType() == ControllerRole::ControllerStepType::FINISHED);
    REQUIRE(controllerReport.passed);
    REQUIRE(controllerReport.pingResponseCounter == CommonSettings::CONTROLLER_PING_COUNT);
    REQUIRE(controllerReport.deviceInfoResponseCounter == static_cast<uint64_t>(1));
    REQUIRE(controllerReport.telemetryReportResponseCounter
            == CommonSettings::CONTROLLER_TELEMETRY_COUNT);
    REQUIRE(controllerReport.verifiedEchoResponseCounter == CommonSettings::CONTROLLER_ECHO_COUNT);
    REQUIRE(controllerReport.corruptedEchoResponseCounter == static_cast<uint64_t>(0));
    REQUIRE(controllerReport.unexpectedEchoResponseCounter == static_cast<uint64_t>(0));
    REQUIRE(controllerReport.busyRetryCounter == static_cast<uint64_t>(0));
    REQUIRE(controllerReport.failedRequestCounter == static_cast<uint64_t>(0));

    // check the result lines of the controller console
    const std::string controllerText = controllerConsoleOutput.getText();
    const std::string deviceInfoText = "log DEBUG: Device info: unit-under-test, firmware "
                                       + std::to_string(AppSettings::AppVersionMajor) + "."
                                       + std::to_string(AppSettings::AppVersionMinor) + ", uptime ";
    REQUIRE(getTextFound(controllerText, "log DEBUG: Device answered after 1 ms\r\n"));
    REQUIRE(getTextFound(controllerText, deviceInfoText));
    REQUIRE(getTextFound(controllerText, "temperature no reading\r\n"));
    REQUIRE(getTextFound(controllerText,
                         "log DEBUG: Stress report: echo responses verified 300, corrupted 0, "
                         "unexpected 0\r\n"));
    REQUIRE(getTextFound(controllerText, "log DEBUG: Controller passed every step\r\n"));
    REQUIRE(
        getTextFound(controllerText,
                     "log DEBUG: Controller report: pings 3, info 1, telemetry 3, echoes 300 of "
                     "300, busy 0, failed 0\r\n"));

    // check that the telemetry stopped after the telemetry step
    REQUIRE(deviceRole->getLinkStatistics().failedDeliveryCounter == static_cast<uint64_t>(0));
    const uint64_t outgoingFrameCounter = deviceRole->getLinkStatistics().outgoingFrameCounter;
    advance(2 * CommonSettings::CONTROLLER_TELEMETRY_PERIOD_MSEC);
    REQUIRE(deviceRole->getLinkStatistics().outgoingFrameCounter == outgoingFrameCounter);
}

TEST_CASE_METHOD(TestControllerRole,
                 "The controller role passes every step when frames get lost on both ways",
                 "[ControllerRole][testLostFrames]")
{
    // drop every fiftieth frame of both roles
    static constexpr size_t droppedSendPeriod = 50;
    const DataFilter dropFilter = [](const size_t sendCount,
                                     [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
        return sendCount % droppedSendPeriod != 0;
    };
    controllerTransport.setFilter(dropFilter);
    deviceTransport.setFilter(dropFilter);

    // run both roles until the controller is finished
    runUntilFinished();

    // check the result with the retransmissions of the lost frames
    const ControllerRole::ControllerReport controllerReport = controllerRole->getControllerReport();
    WARN("retransmissions of the controller: "
         << controllerRole->getLinkStatistics().retransmissionCounter
         << ", busy retries: " << controllerReport.busyRetryCounter);
    REQUIRE(controllerReport.passed);
    REQUIRE(controllerReport.verifiedEchoResponseCounter == CommonSettings::CONTROLLER_ECHO_COUNT);
    REQUIRE(controllerReport.failedRequestCounter == static_cast<uint64_t>(0));
    REQUIRE(controllerRole->getLinkStatistics().retransmissionCounter > static_cast<uint64_t>(0));
    REQUIRE(deviceRole->getLinkStatistics().retransmissionCounter > static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestControllerRole,
                 "The ping step counts only the ping responses to its own ping requests",
                 "[ControllerRole][testLatePingResponses]")
{
    // hold every frame of the controller while the device boots
    std::vector<std::vector<uint8_t>> heldData;
    controllerTransport.setFilter(
        [&heldData]([[maybe_unused]] const size_t sendCount, const std::vector<uint8_t> &data) {
            heldData.push_back(data);
            return false;
        });
    static constexpr size_t heldPeriodCount = 3 * (Settings::DEFAULT_MAXIMUM_RETRANSMISSIONS + 1);
    for (size_t heldPeriodIndex = 0; heldPeriodIndex < heldPeriodCount; ++heldPeriodIndex) {
        advance(Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC);
    }

    // deliver the held ping requests at once when the device starts
    controllerTransport.setFilter(nullptr);
    std::for_each(heldData.cbegin(), heldData.cend(), [this](const std::vector<uint8_t> &data) {
        deviceRole->onReceivedData(data.data(), data.size());
    });
    pump();

    // run both roles until the controller is finished
    runUntilFinished();

    // check that the answers of the held ping requests were not counted
    const ControllerRole::ControllerReport controllerReport = controllerRole->getControllerReport();
    REQUIRE(heldData.size() > static_cast<size_t>(Settings::DEFAULT_MAXIMUM_RETRANSMISSIONS + 1));
    REQUIRE(controllerReport.passed);
    REQUIRE(controllerReport.pingResponseCounter == CommonSettings::CONTROLLER_PING_COUNT);
}

TEST_CASE_METHOD(TestControllerRole,
                 "The controller role fails the connect step without a device",
                 "[ControllerRole][testNoDevice]")
{
    // drop every frame of the controller
    controllerTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });

    // run both roles until the controller is finished
    runUntilFinished();

    // check the failed connect step
    const std::string controllerText = controllerConsoleOutput.getText();
    REQUIRE(controllerRole->getControllerStepType() == ControllerRole::ControllerStepType::FINISHED);
    REQUIRE_FALSE(controllerRole->getControllerReport().passed);
    REQUIRE(getTextFound(controllerText,
                         "log WARNING: Controller step CONNECT timed out after 30000 ms\r\n"));
    REQUIRE(getTextFound(controllerText, "log WARNING: Controller failed in step CONNECT\r\n"));
    REQUIRE(controllerRole->getControllerReport().failedRequestCounter == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestControllerRole,
                 "The controller role fails when a request of a step fails",
                 "[ControllerRole][testFailedRequest]")
{
    // drop every frame of the device from the info step on
    deviceTransport.setFilter([this]([[maybe_unused]] const size_t sendCount,
                                     [[maybe_unused]] const std::vector<uint8_t> &data) {
        return controllerRole->getControllerStepType() != ControllerRole::ControllerStepType::INFO;
    });

    // run both roles until the controller is finished
    runUntilFinished();

    // check the failed device info request
    const std::string controllerText = controllerConsoleOutput.getText();
    REQUIRE_FALSE(controllerRole->getControllerReport().passed);
    REQUIRE(controllerRole->getControllerReport().failedRequestCounter == static_cast<uint64_t>(1));
    REQUIRE(getTextFound(controllerText,
                         "log WARNING: Request DEVICE_INFO_REQUEST failed: TIMED_OUT, message "
                         "status 0\r\n"));
    REQUIRE(getTextFound(controllerText, "log WARNING: Controller failed in step INFO\r\n"));
}

TEST_CASE_METHOD(TestControllerRole,
                 "The controller role prints the log lines of the device",
                 "[ControllerRole][testDeviceLog]")
{
    // report a log line on the device
    deviceRole->reportLog(Messages::LogLevelType::WARNING, "Heartbeat: free memory 4242 bytes");
    pump();

    // check the log line on both consoles
    REQUIRE(getTextFound(deviceConsoleOutput.getText(),
                         "log WARNING: Heartbeat: free memory 4242 bytes\r\n"));
    REQUIRE(getTextFound(controllerConsoleOutput.getText(),
                         "log WARNING: Device log: Heartbeat: free memory 4242 bytes\r\n"));
}

TEST_CASE_METHOD(TestControllerRole,
                 "The device role traces every frame on its printed console",
                 "[ControllerRole][testDeviceTrace]")
{
    // run both roles until the controller is finished
    runUntilFinished();

    // check the traced frames in both directions
    const std::string deviceText = deviceConsoleOutput.getText();
    REQUIRE(getTextFound(deviceText, ">> #0     PING_REQUEST                 ACK_REQUEST"));
    REQUIRE(getTextFound(deviceText, "<< #0     PING_RESPONSE                ACK_REQUEST"));
    REQUIRE(getTextFound(deviceText, "<< #0     PING_REQUEST                 ACK         "));
    REQUIRE_FALSE(getTextFound(deviceText, "CRC bad"));
}

TEST_CASE_METHOD(TestControllerRole,
                 "The LED of both roles blinks on received frames and shows its state afterwards",
                 "[ControllerRole][testActivityLed]")
{
    // run both roles until the controller is finished and the last blink is over
    runUntilFinished();
    advance(CommonSettings::ACTIVITY_BLINK_DURATION_MSEC);

    // check the blinks and the restored LED state
    REQUIRE(controllerPlatform.getShownLedCount() > static_cast<size_t>(0));
    REQUIRE(devicePlatform.getShownLedCount() > static_cast<size_t>(0));
    REQUIRE_FALSE(controllerPlatform.getLedLit());
    REQUIRE_FALSE(devicePlatform.getLedLit());
}

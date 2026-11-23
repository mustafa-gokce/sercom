#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Router.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"
#include "../firmware/common/include/CommonSettings.h"
#include "../firmware/common/include/Console.h"
#include "../firmware/common/include/ConsoleOutput.h"
#include "../firmware/common/include/DeviceRole.h"
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
     * Mock transport that queues the sent data until the test delivers it to the next node.
     */
    MockTransport() = default;
    ~MockTransport() final {}

    // delete copy constructor and copy assignment operator
    MockTransport(const MockTransport &) = delete;
    MockTransport &operator=(const MockTransport &) = delete;

    // delete move constructor and move assignment operator
    MockTransport(MockTransport &&) = delete;
    MockTransport &operator=(MockTransport &&) = delete;

public:
    /*
     * Send data into the queue.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, const size_t size) final
    {
        // queue the data
        this->sentDataQueue.emplace_back(data, data + size);

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

private:
    std::deque<std::vector<uint8_t>> sentDataQueue; // sent data that waits for the next node
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

class MockHostHandler final : public Link::LinkHandler
{
public:
    /*
     * Mock link handler of a host that records every received message.
     */
    MockHostHandler() = default;
    ~MockHostHandler() final {}

    // delete copy constructor and copy assignment operator
    MockHostHandler(const MockHostHandler &) = delete;
    MockHostHandler &operator=(const MockHostHandler &) = delete;

    // delete move constructor and move assignment operator
    MockHostHandler(MockHostHandler &&) = delete;
    MockHostHandler &operator=(MockHostHandler &&) = delete;

public:
    /*
     * Listen for a received message, record it and accept it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame) final
    {
        // record the message
        this->messages.push_back(frame);

        // accept the message
        return Definitions::MessageStatusType::ACCEPTED;
    }

    /*
     * Listen for a confirmed delivery that needs no work.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {}

    /*
     * Listen for a failed delivery that needs no work.
     */
    void onDeliveryFailed(
        [[maybe_unused]] const uint16_t messageCounter,
        [[maybe_unused]] const uint16_t messageType,
        [[maybe_unused]] const Definitions::DeliveryFailureType deliveryFailureType,
        [[maybe_unused]] const Definitions::MessageStatusType remoteMessageStatusType) final
    {}

public:
    /*
     * Get the received messages in arrival order.
     */
    [[nodiscard]] std::vector<Frame::Frame> getMessages() const
    {
        // expose the received messages
        return this->messages;
    }

private:
    std::vector<Frame::Frame> messages; // received messages
};

} // namespace

class TestDeviceRole
{
public:
    /*
     * Device role test fixture with a host, the router role of node 2 and the end node role of node 3.
     */
    TestDeviceRole()
        : messageCounter(0)
    {
        // initialize clock
        static constexpr std::chrono::milliseconds startTime = 5000ms;
        this->clock.setTime(startTime);

        // initialize host
        this->hostRouter.reset(
            new Router::Router(this->hostTransport,
                               nullptr,
                               this->hostHandler,
                               this->clock,
                               Link::LinkConfiguration(),
                               getRouterConfiguration(Definitions::HOST_ADDRESS)));

        // initialize consoles
        this->routerConsole.reset(
            new Console::Console(this->routerConsoleOutput, this->clock, true));
        this->endConsole.reset(new Console::Console(this->endConsoleOutput, this->clock, true));

        // initialize router role and end node role
        this->routerRole.reset(
            new DeviceRole::DeviceRole(this->routerPrimaryTransport,
                                       &this->routerSecondaryTransport,
                                       this->clock,
                                       this->routerPlatform,
                                       *this->routerConsole,
                                       getRouterConfiguration(RouterNodeAddress)));
        this->endRole.reset(new DeviceRole::DeviceRole(this->endTransport,
                                                       this->clock,
                                                       this->endPlatform,
                                                       *this->endConsole,
                                                       getRouterConfiguration(EndNodeAddress)));
    }
    ~TestDeviceRole()
    {
        // close router role and end node role
        this->endRole.reset(nullptr);
        this->routerRole.reset(nullptr);

        // close consoles
        this->endConsole.reset(nullptr);
        this->routerConsole.reset(nullptr);

        // close host
        this->hostRouter.reset(nullptr);
    }

    /*
     * Get the configuration of a router with a node address.
     */
    [[nodiscard]] static Router::RouterConfiguration getRouterConfiguration(const uint8_t nodeAddress)
    {
        // create router configuration
        Router::RouterConfiguration routerConfiguration;
        routerConfiguration.nodeAddress = nodeAddress;

        // expose the router configuration
        return routerConfiguration;
    }

    /*
     * Deliver the data of the host, node 2 and node 3 until no data waits.
     */
    void pump()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->hostTransport.getEmpty() && this->routerPrimaryTransport.getEmpty()
                && this->routerSecondaryTransport.getEmpty() && this->endTransport.getEmpty()) {
                return;
            }

            // deliver the data of both links in both directions
            std::vector<uint8_t> data;
            while (this->hostTransport.receiveData(data)) {
                this->routerRole->onReceivedData(data.data(), data.size());
            }
            while (this->routerPrimaryTransport.receiveData(data)) {
                this->hostRouter->onReceivedData(Definitions::PortType::PRIMARY,
                                                 data.data(),
                                                 data.size());
            }
            while (this->routerSecondaryTransport.receiveData(data)) {
                this->endRole->onReceivedData(data.data(), data.size());
            }
            while (this->endTransport.receiveData(data)) {
                this->routerRole->onReceivedSecondaryData(data.data(), data.size());
            }
        }
    }

    /*
     * Let time pass, maintain the host and both roles and deliver what they sent.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // let time pass
        this->clock.advanceTime(duration);

        // maintain the host and both roles
        this->hostRouter->maintain();
        this->routerRole->maintain();
        this->endRole->maintain();

        // deliver the sent data
        this->pump();
    }

    /*
     * Send an acknowledged ping request of the host to a destination.
     */
    [[nodiscard]] Definitions::SendStatusType sendPingRequest(const uint8_t destinationAddress)
    {
        // send the ping request with the current time
        const Messages::PingRequest pingRequest(this->clock.getTime());
        return this->hostRouter->sendMessage(destinationAddress,
                                             Messages::fromMessageType(
                                                 Messages::MessageType::PING_REQUEST),
                                             pingRequest.getPayload(),
                                             pingRequest.getPayloadLength(),
                                             true,
                                             this->messageCounter);
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
    TestDeviceRole(const TestDeviceRole &other) = delete;
    TestDeviceRole &operator=(const TestDeviceRole &other) = delete;
    TestDeviceRole(TestDeviceRole &&other) noexcept = delete;
    TestDeviceRole &operator=(TestDeviceRole &&other) noexcept = delete;

protected:
    static constexpr uint8_t RouterNodeAddress = 2; // address of node 2 with two ports
    static constexpr uint8_t EndNodeAddress = 3;    // address of node 3 at the end of the chain

protected:
    MockClock clock;                                    // clock of every node
    MockTransport hostTransport;                        // data sent by the host
    MockTransport routerPrimaryTransport;               // data sent up by node 2
    MockTransport routerSecondaryTransport;             // data sent down by node 2
    MockTransport endTransport;                         // data sent up by node 3
    MockHostHandler hostHandler;                        // messages of the host
    MockFirmwarePlatform routerPlatform;                // hardware of node 2
    MockFirmwarePlatform endPlatform;                   // hardware of node 3
    MockConsoleOutput routerConsoleOutput;              // console text of node 2
    MockConsoleOutput endConsoleOutput;                 // console text of node 3
    std::unique_ptr<Router::Router> hostRouter;         // host of the chain
    std::unique_ptr<Console::Console> routerConsole;    // console of node 2
    std::unique_ptr<Console::Console> endConsole;       // console of node 3
    std::unique_ptr<DeviceRole::DeviceRole> routerRole; // router role of node 2
    std::unique_ptr<DeviceRole::DeviceRole> endRole;    // end node role of node 3
    uint16_t messageCounter;                            // message counter of the latest request
};

TEST_CASE_METHOD(TestDeviceRole,
                 "The end node role answers a request of the host through the router role",
                 "[DeviceRole][testEndNode]")
{
    // ask node 3 for its device info through node 2
    const Messages::DeviceInfoRequest deviceInfoRequest;
    REQUIRE(hostRouter->sendMessage(EndNodeAddress,
                                    Messages::fromMessageType(
                                        Messages::MessageType::DEVICE_INFO_REQUEST),
                                    deviceInfoRequest.getPayload(),
                                    deviceInfoRequest.getPayloadLength(),
                                    true,
                                    messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the routed device info of node 3 with its version and address
    const std::vector<Frame::Frame> messages = hostHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(messages.at(0).getRouted());
    REQUIRE(messages.at(0).getSourceAddress() == EndNodeAddress);
    REQUIRE(messages.at(0).getDestinationAddress() == Definitions::HOST_ADDRESS);
    const Messages::DeviceInfoResponse deviceInfoResponse(messages.at(0));
    REQUIRE(deviceInfoResponse.getLikelyValid());
    REQUIRE(deviceInfoResponse.getDeviceName() == "unit-under-test");
    REQUIRE(deviceInfoResponse.getProtocolVersion() == Definitions::ROUTED_PROTOCOL_VERSION);
    REQUIRE(deviceInfoResponse.getNodeAddress() == EndNodeAddress);

    // check that the end node delivered the request without a secondary port
    const Router::RouterStatistics endStatistics = endRole->getRouterStatistics();
    REQUIRE(endStatistics.localFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(endStatistics.forwardedFrameCounter == static_cast<uint64_t>(0));
    REQUIRE(endRole->getLinkStatistics().incomingFrameCounter > static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestDeviceRole,
                 "The router role forwards the frames of other nodes and answers its own",
                 "[DeviceRole][testRouter]")
{
    // ping node 3 through node 2 and node 2 itself
    REQUIRE(sendPingRequest(EndNodeAddress) == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(sendPingRequest(RouterNodeAddress) == Definitions::SendStatusType::SENT);
    pump();

    // check both ping responses with their source addresses
    const std::vector<Frame::Frame> messages = hostHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(2));
    REQUIRE(Messages::toMessageType(messages.at(0).getMessageType())
            == Messages::MessageType::PING_RESPONSE);
    REQUIRE(messages.at(0).getSourceAddress() == EndNodeAddress);
    REQUIRE(Messages::toMessageType(messages.at(1).getMessageType())
            == Messages::MessageType::PING_RESPONSE);
    REQUIRE(messages.at(1).getSourceAddress() == RouterNodeAddress);

    // check that node 2 forwarded the request down and the response up and delivered its own
    const Router::RouterStatistics routerStatistics = routerRole->getRouterStatistics();
    REQUIRE(routerStatistics.forwardedFrameCounter == static_cast<uint64_t>(2));
    REQUIRE(routerStatistics.localFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(routerStatistics.unreachableFrameCounter == static_cast<uint64_t>(0));

    // check the traced primary port of node 2 with the routes of both directions
    const std::string routerText = routerConsoleOutput.getText();
    REQUIRE(getTextFound(routerText, "PING_REQUEST                 ACK_REQUEST|ROUTE_CAPABLE"));
    REQUIRE(getTextFound(routerText, "CRC ok  route 1>3 ttl 8\r\n"));
    REQUIRE(getTextFound(routerText, "CRC ok  route 3>1 ttl 7\r\n"));
    REQUIRE(getTextFound(routerText, "CRC ok  route 2>1 ttl 8\r\n"));
    REQUIRE_FALSE(getTextFound(routerText, "CRC bad"));
}

TEST_CASE_METHOD(TestDeviceRole,
                 "The LED of the router role blinks on the valid frames of both ports",
                 "[DeviceRole][testActivityLed]")
{
    // send a log line of node 3 up to the secondary port of node 2 only
    endRole->reportLog(Messages::LogLevelType::DEBUG, "Started the DeviceApplication");
    std::vector<uint8_t> data;
    while (endTransport.receiveData(data)) {
        routerRole->onReceivedSecondaryData(data.data(), data.size());
    }
    routerRole->maintain();

    // check the blink after a frame of the secondary port alone
    REQUIRE(routerRole->getLinkStatistics().incomingFrameCounter == static_cast<uint64_t>(0));
    REQUIRE(routerPlatform.getShownLedCount() == static_cast<size_t>(1));
    REQUIRE(routerPlatform.getLedLit());
    advance(CommonSettings::ACTIVITY_BLINK_PERIOD_MSEC);
    REQUIRE_FALSE(routerPlatform.getLedLit());

    // send a ping request of the host to the primary port of node 2
    const size_t shownLedCount = routerPlatform.getShownLedCount();
    REQUIRE(sendPingRequest(RouterNodeAddress) == Definitions::SendStatusType::SENT);
    while (hostTransport.receiveData(data)) {
        routerRole->onReceivedData(data.data(), data.size());
    }
    routerRole->maintain();

    // check the blink after a frame of the primary port
    REQUIRE(routerPlatform.getShownLedCount() == shownLedCount + 1);
    REQUIRE(routerPlatform.getLedLit());
}

TEST_CASE_METHOD(TestDeviceRole,
                 "The log lines of a node with an address go to the host address",
                 "[DeviceRole][testLogToHost]")
{
    // report a log line on node 3 before any request
    endRole->reportLog(Messages::LogLevelType::WARNING, "Heartbeat: free memory 4242 bytes");
    pump();

    // check the routed log report response at the host
    const std::vector<Frame::Frame> messages = hostHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(messages.at(0).getRouted());
    REQUIRE(messages.at(0).getSourceAddress() == EndNodeAddress);
    REQUIRE(messages.at(0).getDestinationAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(messages.at(0).getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE - 1);
    const Messages::LogReportResponse logReportResponse(messages.at(0));
    REQUIRE(logReportResponse.getLogLevelType() == Messages::LogLevelType::WARNING);
    REQUIRE(logReportResponse.getText() == "Heartbeat: free memory 4242 bytes");

    // check the console of node 3 and the traced forward of node 2
    REQUIRE(getTextFound(endConsoleOutput.getText(),
                         "log WARNING: Heartbeat: free memory 4242 bytes\r\n"));
    REQUIRE(getTextFound(endConsoleOutput.getText(), "CRC ok  route 3>1 ttl 8\r\n"));
    REQUIRE(
        getTextFound(routerConsoleOutput.getText(), "LOG_REPORT_RESPONSE          ROUTE_CAPABLE"));
    REQUIRE(getTextFound(routerConsoleOutput.getText(), "CRC ok  route 3>1 ttl 7\r\n"));
}

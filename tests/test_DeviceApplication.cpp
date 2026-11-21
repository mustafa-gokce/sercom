#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../app/include/AppSettings.h"
#include "../app/include/DeviceApplication.h"
#include "../app/include/DeviceClient.h"
#include "../app/include/Messages.h"
#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Settings.h"
#include "../core/include/Transport.h"

using namespace std::chrono_literals;
using namespace sercom;

namespace {

using DataFilter = std::function<bool(size_t sendCount, const std::vector<uint8_t> &data)>;

/*
 * Acknowledged message that a mock link handler saw finish.
 */
struct DeliveryResult
{
    Definitions::MessageStatusType remoteMessageStatusType
        = Definitions::MessageStatusType::UNKNOWN; // message status of a rejection
};

/*
 * Request that a mock client handler saw fail.
 */
struct FailedRequest
{
    uint16_t messageCounter = 0; // message counter of the request
    Definitions::MessageStatusType remoteMessageStatusType
        = Definitions::MessageStatusType::UNKNOWN; // message status of a rejection
};

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
     * Mock transport that queues the sent data until the test receives it.
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

    /*
     * Drop the queued data, the filter and the number of sends.
     */
    void clear()
    {
        // drop every state
        this->sentDataQueue.clear();
        this->filter = nullptr;
        this->sendCount = 0;
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
    std::deque<std::vector<uint8_t>> sentDataQueue; // sent data that waits for the receiver
    DataFilter filter;                              // filter that keeps or drops sent data
    size_t sendCount;                               // number of sends so far
};

class MockLinkHandler final : public Link::LinkHandler
{
public:
    /*
     * Mock link handler that records every finished delivery and accepts every message.
     */
    MockLinkHandler() = default;
    ~MockLinkHandler() final {}

    // delete copy constructor and copy assignment operator
    MockLinkHandler(const MockLinkHandler &) = delete;
    MockLinkHandler &operator=(const MockLinkHandler &) = delete;

    // delete move constructor and move assignment operator
    MockLinkHandler(MockLinkHandler &&) = delete;
    MockLinkHandler &operator=(MockLinkHandler &&) = delete;

public:
    /*
     * Listen for a received message and accept it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(
        [[maybe_unused]] const Frame::Frame &frame) final
    {
        // accept the message
        return Definitions::MessageStatusType::ACCEPTED;
    }

    /*
     * Listen for a confirmed delivery and record it.
     */
    void onDeliveryConfirmed([[maybe_unused]] const uint16_t messageCounter,
                             [[maybe_unused]] const uint16_t messageType) final
    {
        // record the confirmed delivery
        this->deliveryResults.push_back(DeliveryResult());
    }

    /*
     * Listen for a failed delivery and record it.
     */
    void onDeliveryFailed([[maybe_unused]] const uint16_t messageCounter,
                          [[maybe_unused]] const uint16_t messageType,
                          [[maybe_unused]] const Definitions::DeliveryFailureType deliveryFailureType,
                          const Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // record the failed delivery
        DeliveryResult deliveryResult;
        deliveryResult.remoteMessageStatusType = remoteMessageStatusType;
        this->deliveryResults.push_back(deliveryResult);
    }

public:
    /*
     * Get the delivery results in arrival order.
     */
    [[nodiscard]] std::vector<DeliveryResult> getDeliveryResults() const
    {
        // expose the delivery results
        return this->deliveryResults;
    }

private:
    std::vector<DeliveryResult> deliveryResults; // finished deliveries
};

class MockDevicePlatform final : public DeviceApplication::DevicePlatform
{
public:
    /*
     * Mock device platform with a fixed device and an LED that counts its switches.
     */
    MockDevicePlatform()
        : ledOn(false)
        , ledSwitchCount(0)
    {}
    ~MockDevicePlatform() final {}

    // delete copy constructor and copy assignment operator
    MockDevicePlatform(const MockDevicePlatform &) = delete;
    MockDevicePlatform &operator=(const MockDevicePlatform &) = delete;

    // delete move constructor and move assignment operator
    MockDevicePlatform(MockDevicePlatform &&) = delete;
    MockDevicePlatform &operator=(MockDevicePlatform &&) = delete;

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
        // set whether the LED is on
        this->ledOn = ledOn;

        // keep track of LED switches
        ++this->ledSwitchCount;
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
        // expose a fixed temperature
        static constexpr int16_t temperature = -550;
        return temperature;
    }

    /*
     * Get the device name.
     */
    [[nodiscard]] std::string_view getDeviceName() const final
    {
        // expose a fixed device name
        return {"unit-under-test"};
    }

public:
    /*
     * Get the number of LED switches.
     */
    [[nodiscard]] size_t getLedSwitchCount() const
    {
        // expose the number of LED switches
        return this->ledSwitchCount;
    }

private:
    bool ledOn;            // LED is on
    size_t ledSwitchCount; // number of LED switches
};

class MockClientHandler final : public DeviceClient::DeviceClientHandler
{
public:
    /*
     * Mock client handler that records every device response and every finished request.
     */
    MockClientHandler() = default;
    ~MockClientHandler() final {}

    // delete copy constructor and copy assignment operator
    MockClientHandler(const MockClientHandler &) = delete;
    MockClientHandler &operator=(const MockClientHandler &) = delete;

    // delete move constructor and move assignment operator
    MockClientHandler(MockClientHandler &&) = delete;
    MockClientHandler &operator=(MockClientHandler &&) = delete;

public:
    /*
     * Listen for a ping response and record it with its round trip time.
     */
    void onReceivedPingResponse(const Messages::PingResponse &pingResponse,
                                const std::chrono::milliseconds roundTripTime) final
    {
        // record the ping response
        this->pingResponses.push_back(pingResponse);
        this->roundTripTimes.push_back(roundTripTime);
    }

    /*
     * Listen for a device info response and record it.
     */
    void onReceivedDeviceInfoResponse(const Messages::DeviceInfoResponse &deviceInfoResponse) final
    {
        // record the device info response
        this->deviceInfoResponses.push_back(deviceInfoResponse);
    }

    /*
     * Listen for a set LED response and record it.
     */
    void onReceivedSetLedResponse(const Messages::SetLedResponse &setLedResponse) final
    {
        // record the set LED response
        this->setLedResponses.push_back(setLedResponse);
    }

    /*
     * Listen for a telemetry report response and record it.
     */
    void onReceivedTelemetryReportResponse(
        const Messages::TelemetryReportResponse &telemetryReportResponse) final
    {
        // record the telemetry report response
        this->telemetryReportResponses.push_back(telemetryReportResponse);
    }

    /*
     * Listen for a log report response and record it.
     */
    void onReceivedLogReportResponse(const Messages::LogReportResponse &logReportResponse) final
    {
        // record the log report response
        this->logReportResponses.push_back(logReportResponse);
    }

    /*
     * Listen for an echo response and record it.
     */
    void onReceivedEchoResponse(const Messages::EchoResponse &echoResponse) final
    {
        // record the echo response
        this->echoResponses.push_back(echoResponse);
    }

    /*
     * Listen for a confirmed request and record its message type.
     */
    void onRequestConfirmed([[maybe_unused]] const uint16_t messageCounter,
                            const Messages::MessageType messageType) final
    {
        // record the message type
        this->confirmedMessageTypes.push_back(messageType);
    }

    /*
     * Listen for a failed request and record it.
     */
    void onRequestFailed(const uint16_t messageCounter,
                         [[maybe_unused]] const Messages::MessageType messageType,
                         [[maybe_unused]] const Definitions::DeliveryFailureType deliveryFailureType,
                         const Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // record the failed request
        FailedRequest failedRequest;
        failedRequest.messageCounter = messageCounter;
        failedRequest.remoteMessageStatusType = remoteMessageStatusType;
        this->failedRequests.push_back(failedRequest);
    }

    /*
     * Drop every recorded response and request.
     */
    void clear()
    {
        // drop every record
        this->pingResponses.clear();
        this->roundTripTimes.clear();
        this->deviceInfoResponses.clear();
        this->setLedResponses.clear();
        this->telemetryReportResponses.clear();
        this->logReportResponses.clear();
        this->echoResponses.clear();
        this->confirmedMessageTypes.clear();
        this->failedRequests.clear();
    }

public:
    /*
     * Get the ping responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::PingResponse> getPingResponses() const
    {
        // expose the ping responses
        return this->pingResponses;
    }

    /*
     * Get the round trip times of the ping responses.
     */
    [[nodiscard]] std::vector<std::chrono::milliseconds> getRoundTripTimes() const
    {
        // expose the round trip times
        return this->roundTripTimes;
    }

    /*
     * Get the device info responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::DeviceInfoResponse> getDeviceInfoResponses() const
    {
        // expose the device info responses
        return this->deviceInfoResponses;
    }

    /*
     * Get the set LED responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::SetLedResponse> getSetLedResponses() const
    {
        // expose the set LED responses
        return this->setLedResponses;
    }

    /*
     * Get the telemetry report responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::TelemetryReportResponse> getTelemetryReportResponses() const
    {
        // expose the telemetry report responses
        return this->telemetryReportResponses;
    }

    /*
     * Get the log report responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::LogReportResponse> getLogReportResponses() const
    {
        // expose the log report responses
        return this->logReportResponses;
    }

    /*
     * Get the echo responses in arrival order.
     */
    [[nodiscard]] std::vector<Messages::EchoResponse> getEchoResponses() const
    {
        // expose the echo responses
        return this->echoResponses;
    }

    /*
     * Get the message types of the confirmed requests.
     */
    [[nodiscard]] std::vector<Messages::MessageType> getConfirmedMessageTypes() const
    {
        // expose the message types of the confirmed requests
        return this->confirmedMessageTypes;
    }

    /*
     * Get the failed requests in arrival order.
     */
    [[nodiscard]] std::vector<FailedRequest> getFailedRequests() const
    {
        // expose the failed requests
        return this->failedRequests;
    }

private:
    std::vector<Messages::PingResponse> pingResponses;             // received ping responses
    std::vector<std::chrono::milliseconds> roundTripTimes;         // measured round trip times
    std::vector<Messages::DeviceInfoResponse> deviceInfoResponses; // received device info
    std::vector<Messages::SetLedResponse> setLedResponses;         // received LED states
    std::vector<Messages::TelemetryReportResponse> telemetryReportResponses; // received telemetry
    std::vector<Messages::LogReportResponse> logReportResponses;             // received log texts
    std::vector<Messages::EchoResponse> echoResponses;        // received echo responses
    std::vector<Messages::MessageType> confirmedMessageTypes; // confirmed requests
    std::vector<FailedRequest> failedRequests;                // failed requests
};

} // namespace

class TestDeviceApplication
{
public:
    /*
     * Device application test fixture.
     */
    TestDeviceApplication()
        : messageCounter(0)
    {
        // initialize clocks
        static constexpr std::chrono::milliseconds startTime = 5000ms;
        static constexpr std::chrono::milliseconds previousStartTime = 900000ms;
        this->clock.setTime(startTime);
        this->previousClock.setTime(previousStartTime);

        // initialize device client and device application
        this->deviceClient.reset(
            new DeviceClient::DeviceClient(this->hostTransport, this->clock, this->clientHandler));
        this->deviceApplication.reset(
            new DeviceApplication::DeviceApplication(this->deviceTransport,
                                                     this->clock,
                                                     this->devicePlatform));

        // initialize previous device client
        this->previousDeviceClient.reset(
            new DeviceClient::DeviceClient(this->hostTransport,
                                           this->previousClock,
                                           this->previousClientHandler));

        // initialize host link
        this->hostLink.reset(
            new Link::Link(this->hostLinkTransport, this->hostLinkHandler, this->clock));
    }
    ~TestDeviceApplication()
    {
        // close host link
        this->hostLink.reset(nullptr);

        // close previous device client
        this->previousDeviceClient.reset(nullptr);

        // close device client and device application
        this->deviceApplication.reset(nullptr);
        this->deviceClient.reset(nullptr);
    }

    /*
     * Get the payload bytes of a message.
     */
    [[nodiscard]] static std::vector<uint8_t> getPayload(
        const Messages::GenericMessage &genericMessage)
    {
        // expose a copy of the payload bytes
        return std::vector<uint8_t>(genericMessage.getPayload(),
                                    genericMessage.getPayload() + genericMessage.getPayloadLength());
    }

    /*
     * Deliver the data of the device client and the device application until no data waits.
     */
    void pump()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->hostTransport.getEmpty() && this->deviceTransport.getEmpty()) {
                return;
            }

            // deliver the data of the host to the device application
            std::vector<uint8_t> data;
            while (this->hostTransport.receiveData(data)) {
                this->deviceApplication->onReceivedData(data.data(), data.size());
            }

            // deliver the data of the device to the device client
            while (this->deviceTransport.receiveData(data)) {
                this->deviceClient->onReceivedData(data.data(), data.size());
            }
        }
    }

    /*
     * Deliver the sent data between the host link and the device application until no data waits.
     */
    void pumpHostLink()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->hostLinkTransport.getEmpty() && this->deviceTransport.getEmpty()) {
                return;
            }

            // deliver the host link data to the device application
            std::vector<uint8_t> data;
            while (this->hostLinkTransport.receiveData(data)) {
                this->deviceApplication->onReceivedData(data.data(), data.size());
            }

            // deliver the data of the device to the host link
            while (this->deviceTransport.receiveData(data)) {
                this->hostLink->onReceivedData(data.data(), data.size());
            }
        }
    }

    /*
     * Let time pass, maintain both sides and deliver what they sent.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // let time pass
        this->clock.advanceTime(duration);

        // maintain both sides
        this->deviceClient->maintain();
        this->deviceApplication->maintain();

        // deliver the sent data
        this->pump();
    }

    /*
     * Restart the device client and the device application with cleared mocks.
     */
    void restart()
    {
        // close device client and device application
        this->deviceApplication.reset(nullptr);
        this->deviceClient.reset(nullptr);

        // clear the mocks of both sides
        this->hostTransport.clear();
        this->deviceTransport.clear();
        this->clientHandler.clear();

        // initialize device client and device application
        this->deviceClient.reset(
            new DeviceClient::DeviceClient(this->hostTransport, this->clock, this->clientHandler));
        this->deviceApplication.reset(
            new DeviceApplication::DeviceApplication(this->deviceTransport,
                                                     this->clock,
                                                     this->devicePlatform));
    }

    // delete copy and move operators
    TestDeviceApplication(const TestDeviceApplication &other) = delete;
    TestDeviceApplication &operator=(const TestDeviceApplication &other) = delete;
    TestDeviceApplication(TestDeviceApplication &&other) noexcept = delete;
    TestDeviceApplication &operator=(TestDeviceApplication &&other) noexcept = delete;

protected:
    MockClock clock;                         // clock of the device client and the device
    MockClock previousClock;                 // clock of a previous host process
    MockTransport hostTransport;             // data sent by the device clients
    MockTransport deviceTransport;           // data sent by the device application
    MockTransport hostLinkTransport;         // data sent by the host link
    MockDevicePlatform devicePlatform;       // hardware of the device
    MockClientHandler clientHandler;         // events of the device client
    MockClientHandler previousClientHandler; // events of the previous device client
    MockLinkHandler hostLinkHandler;         // events of the host link
    std::unique_ptr<DeviceClient::DeviceClient> deviceClient;                // host side
    std::unique_ptr<DeviceApplication::DeviceApplication> deviceApplication; // device side
    std::unique_ptr<DeviceClient::DeviceClient> previousDeviceClient; // host side of an old process
    std::unique_ptr<Link::Link> hostLink; // host side without a device client
    uint16_t messageCounter;              // message counter of the latest request
};

TEST_CASE_METHOD(TestDeviceApplication,
                 "A ping request is answered with a ping response carrying the request timestamp",
                 "[DeviceApplication][testPingRequest]")
{
    // send a ping request and wait the round trip time
    static constexpr std::chrono::milliseconds expectedRoundTripTime = 7ms;
    const std::chrono::milliseconds requestTimestamp = clock.getTime();
    REQUIRE(deviceClient->sendPingRequest(messageCounter) == Definitions::SendStatusType::SENT);
    clock.advanceTime(expectedRoundTripTime);
    pump();

    // check the ping response on the host
    const std::vector<Messages::PingResponse> pingResponses = clientHandler.getPingResponses();
    REQUIRE(pingResponses.size() == static_cast<size_t>(1));
    REQUIRE(pingResponses.at(0).getRequestTimestamp() == requestTimestamp);
    REQUIRE(pingResponses.at(0).getUptime() == expectedRoundTripTime);
    REQUIRE(clientHandler.getRoundTripTimes().at(0) == expectedRoundTripTime);
    REQUIRE(clientHandler.getConfirmedMessageTypes()
            == std::vector<Messages::MessageType>({Messages::MessageType::PING_REQUEST}));
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A ping response for a ping request of a previous host process is ignored",
                 "[DeviceApplication][testPreviousPingResponse]")
{
    // send a ping request of a previous host process
    REQUIRE(previousDeviceClient->sendPingRequest(messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check that no round trip is reported
    REQUIRE(clientHandler.getPingResponses().empty());
    REQUIRE(clientHandler.getRoundTripTimes().empty());
    REQUIRE(deviceClient->getIgnoredPingResponseCounter() == static_cast<uint64_t>(1));

    // check that the acknowledged ping response is not sent again
    advance(4 * Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC);
    REQUIRE(deviceClient->getIgnoredPingResponseCounter() == static_cast<uint64_t>(1));
    REQUIRE(deviceApplication->getLinkStatistics().retransmissionCounter
            == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "Ping requests after an ignored ping response are measured normally",
                 "[DeviceApplication][testPingAfterIgnoredPingResponse]")
{
    // send a ping request of a previous host process
    REQUIRE(previousDeviceClient->sendPingRequest(messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // send three rounds of ping requests of the device client
    static constexpr std::chrono::milliseconds expectedRoundTripTime = 3ms;
    static constexpr size_t pingRequestCount = 3 * AppSettings::PING_HISTORY_SIZE;
    for (size_t pingRequestIndex = 0; pingRequestIndex < pingRequestCount; ++pingRequestIndex) {
        CAPTURE(pingRequestIndex);
        REQUIRE(deviceClient->sendPingRequest(messageCounter) == Definitions::SendStatusType::SENT);
        clock.advanceTime(expectedRoundTripTime);
        pump();
    }

    // check every round trip time on the device client clock
    const std::vector<std::chrono::milliseconds> roundTripTimes = clientHandler.getRoundTripTimes();
    REQUIRE(clientHandler.getPingResponses().size() == pingRequestCount);
    REQUIRE(std::all_of(roundTripTimes.cbegin(),
                        roundTripTimes.cend(),
                        [](const std::chrono::milliseconds roundTripTime) {
                            return roundTripTime == expectedRoundTripTime;
                        }));
    REQUIRE(deviceClient->getIgnoredPingResponseCounter() == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "Device info reports the version, the free heap size and the device name",
                 "[DeviceApplication][testDeviceInfoRequest]")
{
    // send a device info request
    REQUIRE(deviceClient->sendDeviceInfoRequest(messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the device info response on the host
    const std::vector<Messages::DeviceInfoResponse> deviceInfoResponses
        = clientHandler.getDeviceInfoResponses();
    REQUIRE(deviceInfoResponses.size() == static_cast<size_t>(1));
    REQUIRE(deviceInfoResponses.at(0).getFirmwareVersionMajor() == AppSettings::AppVersionMajor);
    REQUIRE(deviceInfoResponses.at(0).getFirmwareVersionMinor() == AppSettings::AppVersionMinor);
    REQUIRE(deviceInfoResponses.at(0).getFreeHeapSize() == devicePlatform.getFreeHeapSize());
    REQUIRE(deviceInfoResponses.at(0).getDeviceName() == devicePlatform.getDeviceName());
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "The LED follows the set LED request and the response shows the real state",
                 "[DeviceApplication][testSetLedRequest]")
{
    // switch the LED on
    REQUIRE(deviceClient->sendSetLedRequest(true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(devicePlatform.getLedOn());
    REQUIRE(clientHandler.getSetLedResponses().size() == static_cast<size_t>(1));
    REQUIRE(clientHandler.getSetLedResponses().back().getLedOn());

    // switch the LED off
    REQUIRE(deviceClient->sendSetLedRequest(false, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE_FALSE(devicePlatform.getLedOn());
    REQUIRE(clientHandler.getSetLedResponses().size() == static_cast<size_t>(2));
    REQUIRE_FALSE(clientHandler.getSetLedResponses().back().getLedOn());
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "An echo request comes back byte for byte",
                 "[DeviceApplication][testEchoRequest]")
{
    // send an echo request
    const std::vector<uint8_t> data = {0xA5, 0xC3, 0x00, 0xFF, 0x42};
    REQUIRE(deviceClient->sendEchoRequest(data.data(), data.size(), messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the echo response on the host
    const std::vector<Messages::EchoResponse> echoResponses = clientHandler.getEchoResponses();
    REQUIRE(echoResponses.size() == static_cast<size_t>(1));
    REQUIRE(getPayload(echoResponses.at(0)) == data);
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "Telemetry starts, follows the telemetry period and stops",
                 "[DeviceApplication][testTelemetry]")
{
    // check that the telemetry is off by default
    static constexpr std::chrono::milliseconds telemetryPeriod = 100ms;
    advance(10 * telemetryPeriod);
    REQUIRE(clientHandler.getTelemetryReportResponses().empty());

    // start the telemetry
    REQUIRE(deviceClient->sendSetTelemetryPeriodRequest(telemetryPeriod, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(deviceApplication->getTelemetryPeriod() == telemetryPeriod);

    // let five telemetry periods pass in half periods
    static constexpr int halfPeriodCount = 10;
    for (int halfPeriod = 0; halfPeriod < halfPeriodCount; ++halfPeriod) {
        advance(telemetryPeriod / 2);
    }

    // check the telemetry report responses on the host
    const std::vector<Messages::TelemetryReportResponse> telemetryReportResponses
        = clientHandler.getTelemetryReportResponses();
    REQUIRE(telemetryReportResponses.size() == static_cast<size_t>(halfPeriodCount / 2));
    REQUIRE(telemetryReportResponses.at(0).getSequence() == 0);
    REQUIRE(telemetryReportResponses.at(4).getSequence() == 4);
    REQUIRE(telemetryReportResponses.at(0).getTemperature() == devicePlatform.getTemperature());

    // stop the telemetry
    REQUIRE(deviceClient->sendSetTelemetryPeriodRequest(0ms, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    advance(10 * telemetryPeriod);
    REQUIRE(clientHandler.getTelemetryReportResponses().size()
            == static_cast<size_t>(halfPeriodCount / 2));
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A too short telemetry period is rejected and keeps the telemetry period",
                 "[DeviceApplication][testTooShortTelemetryPeriod]")
{
    // set a telemetry period
    static constexpr std::chrono::milliseconds telemetryPeriod = 100ms;
    REQUIRE(deviceClient->sendSetTelemetryPeriodRequest(telemetryPeriod, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(deviceApplication->getTelemetryPeriod() == telemetryPeriod);

    // send a telemetry period below the minimum telemetry period
    REQUIRE(deviceClient->sendSetTelemetryPeriodRequest(AppSettings::MINIMUM_TELEMETRY_PERIOD_MSEC
                                                            - 1ms,
                                                        messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the rejection on both ends
    const std::vector<FailedRequest> failedRequests = clientHandler.getFailedRequests();
    REQUIRE(deviceApplication->getTelemetryPeriod() == telemetryPeriod);
    REQUIRE(failedRequests.size() == static_cast<size_t>(1));
    REQUIRE(failedRequests.back().messageCounter == messageCounter);
    REQUIRE(failedRequests.back().remoteMessageStatusType
            == Definitions::MessageStatusType::INVALID_ARGUMENT);
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A telemetry period outside its field is not sent",
                 "[DeviceApplication][testTelemetryPeriodOutOfRange]")
{
    // send a telemetry period above the field
    REQUIRE(
        deviceClient->sendSetTelemetryPeriodRequest(Messages::MAXIMUM_TELEMETRY_PERIOD_MSEC + 1ms,
                                                    messageCounter)
        == Definitions::SendStatusType::INVALID_ARGUMENT);

    // check that nothing left the device client
    REQUIRE(hostTransport.getEmpty());
    REQUIRE(deviceClient->getPendingCount() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A log text above the maximum log text length is not sent",
                 "[DeviceApplication][testLongLogText]")
{
    // maximum log text must fit into a payload
    if (Messages::LOG_REPORT_RESPONSE_SIZE + Messages::MAXIMUM_LOG_TEXT_LENGTH
        > Settings::MaximumPayloadSize) {
        SKIP("The maximum log text does not fit into a payload of this build");
    }

    // send a log text above the maximum log text length
    static constexpr size_t extraLength = 10;
    const std::string text(Messages::MAXIMUM_LOG_TEXT_LENGTH + extraLength, 'x');
    REQUIRE_FALSE(deviceApplication->sendLogReportResponse(Messages::LogLevelType::WARNING, text));

    // check that nothing left the device application
    REQUIRE(deviceTransport.getEmpty());
    pump();

    // check that no log report response reached the host
    REQUIRE(clientHandler.getLogReportResponses().empty());
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "Device log report responses reach the host",
                 "[DeviceApplication][testLogReportResponse]")
{
    // send a log report response
    REQUIRE(deviceApplication->sendLogReportResponse(Messages::LogLevelType::DEBUG, "booted"));
    pump();

    // check the log report response on the host
    const std::vector<Messages::LogReportResponse> logReportResponses
        = clientHandler.getLogReportResponses();
    REQUIRE(logReportResponses.size() == static_cast<size_t>(1));
    REQUIRE(logReportResponses.at(0).getLogLevelType() == Messages::LogLevelType::DEBUG);
    REQUIRE(logReportResponses.at(0).getText() == "booted");
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A malformed set LED request is rejected and nothing switches",
                 "[DeviceApplication][testMalformedSetLedRequest]")
{
    // send a set LED request with an invalid LED state
    const std::vector<uint8_t> payload = {7};
    REQUIRE(hostLink->sendMessage(Messages::fromMessageType(Messages::MessageType::SET_LED_REQUEST),
                                  payload.data(),
                                  payload.size(),
                                  true,
                                  messageCounter)
            == Definitions::SendStatusType::SENT);
    pumpHostLink();

    // check the rejection on both ends
    const std::vector<DeliveryResult> deliveryResults = hostLinkHandler.getDeliveryResults();
    REQUIRE(devicePlatform.getLedSwitchCount() == static_cast<size_t>(0));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType
            == Definitions::MessageStatusType::MALFORMED_PAYLOAD);
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "An unknown message type is rejected as unsupported",
                 "[DeviceApplication][testUnknownMessageType]")
{
    // send a message of an unknown message type
    REQUIRE(hostLink->sendMessage(0x7ABC, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pumpHostLink();

    // check the rejection on the host link
    const std::vector<DeliveryResult> deliveryResults = hostLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType
            == Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE);
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "A device with a full response window answers busy instead of acting",
                 "[DeviceApplication][testFullResponseWindow]")
{
    // lose every response of the device but keep its acknowledgements
    deviceTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount, const std::vector<uint8_t> &data) noexcept {
            const Frame::Frame frame(data.data(), data.size());
            return frame.getFlag(Definitions::FrameFlagType::ACK)
                   || frame.getFlag(Definitions::FrameFlagType::NACK);
        });

    // fill the response window of the device
    for (size_t requestIndex = 0; requestIndex < Settings::MaximumPendingFrames; ++requestIndex) {
        CAPTURE(requestIndex);
        REQUIRE(deviceClient->sendSetLedRequest(true, messageCounter)
                == Definitions::SendStatusType::SENT);
        pump();
    }
    REQUIRE(devicePlatform.getLedSwitchCount() == Settings::MaximumPendingFrames);

    // send the next request while the responses flow again
    deviceTransport.setFilter(DataFilter());
    REQUIRE(deviceClient->sendSetLedRequest(false, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check that the device answers busy without switching
    const std::vector<FailedRequest> failedRequests = clientHandler.getFailedRequests();
    REQUIRE(devicePlatform.getLedSwitchCount() == Settings::MaximumPendingFrames);
    REQUIRE_FALSE(failedRequests.empty());
    REQUIRE(failedRequests.back().remoteMessageStatusType == Definitions::MessageStatusType::BUSY);
}

TEST_CASE_METHOD(TestDeviceApplication,
                 "Requests and responses survive heavy frame loss in both directions",
                 "[DeviceApplication][testHeavyFrameLoss]")
{
    // repeat the exchange over a lossy line for every seed
    static constexpr uint32_t seedCount = 100;
    static constexpr double lossProbability = 0.1;
    static constexpr uint8_t requestCount = 30;
    static constexpr int maximumStepCount = 2000;
    static constexpr int finalStepCount = 200;
    static constexpr std::chrono::milliseconds stepDuration
        = Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC / 5;
    for (uint32_t seed = 1; seed <= seedCount; ++seed) {
        // restart both sides behind a loss filter of the seed
        CAPTURE(seed);
        restart();
        std::mt19937 generator(seed);
        std::uniform_real_distribution<double> lossDistribution(0.0, 1.0);
        const auto lossFilter =
            [&generator, &lossDistribution]([[maybe_unused]] const size_t sendCount,
                                            [[maybe_unused]] const std::vector<uint8_t> &data) {
                return lossDistribution(generator) >= lossProbability;
            };
        hostTransport.setFilter(lossFilter);
        deviceTransport.setFilter(lossFilter);

        // list the identifiers of the requests to send
        std::vector<uint8_t> waitingIdentifiers(requestCount);
        std::iota(waitingIdentifiers.begin(), waitingIdentifiers.end(), static_cast<uint8_t>(0));
        std::vector<uint8_t> messageCounterIdentifiers(static_cast<size_t>(
                                                           std::numeric_limits<uint16_t>::max())
                                                           + 1,
                                                       0);
        std::vector<size_t> timedOutResendCounts(requestCount, 0);
        size_t handledFailureCount = 0;

        // send and resend the requests until every request is finished
        const auto getFinished = [this, &waitingIdentifiers, &handledFailureCount]() {
            return waitingIdentifiers.empty() && deviceClient->getPendingCount() == 0
                   && handledFailureCount == clientHandler.getFailedRequests().size();
        };
        int stepCount = 0;
        while (stepCount < maximumStepCount && !getFinished()) {
            // resend the failed requests (busy ones never reached the application)
            const std::vector<FailedRequest> failedRequests = clientHandler.getFailedRequests();
            std::for_each(failedRequests.cbegin() + static_cast<ptrdiff_t>(handledFailureCount),
                          failedRequests.cend(),
                          [&messageCounterIdentifiers, &timedOutResendCounts, &waitingIdentifiers](
                              const FailedRequest &failedRequest) {
                              // remember the timed out resends
                              const uint8_t identifier = messageCounterIdentifiers.at(
                                  failedRequest.messageCounter);
                              if (failedRequest.remoteMessageStatusType
                                  != Definitions::MessageStatusType::BUSY) {
                                  ++timedOutResendCounts.at(identifier);
                              }

                              // resend the request
                              waitingIdentifiers.push_back(identifier);
                          });
            handledFailureCount = failedRequests.size();

            // send the next waiting request
            if (!waitingIdentifiers.empty() && deviceClient->getSendRequestAllowed()) {
                const std::vector<uint8_t> data = {waitingIdentifiers.front()};
                uint16_t echoMessageCounter = 0;
                if (deviceClient->sendEchoRequest(data.data(), data.size(), echoMessageCounter)
                    == Definitions::SendStatusType::SENT) {
                    messageCounterIdentifiers.at(echoMessageCounter) = waitingIdentifiers.front();
                    waitingIdentifiers.erase(waitingIdentifiers.begin());
                }
            }

            // let a step pass
            advance(stepDuration);
            ++stepCount;
        }

        // let the device retransmit its last responses
        for (int step = 0; step < finalStepCount; ++step) {
            advance(stepDuration);
        }

        // check that nothing waits or flies
        REQUIRE(waitingIdentifiers.empty());
        REQUIRE(deviceClient->getPendingCount() == static_cast<size_t>(0));

        // count the echo responses of every request
        const std::vector<Messages::EchoResponse> echoResponses = clientHandler.getEchoResponses();
        std::vector<size_t> echoResponseCounts(requestCount, 0);
        std::for_each(echoResponses.cbegin(),
                      echoResponses.cend(),
                      [&echoResponseCounts](const Messages::EchoResponse &echoResponse) {
                          // count the echo response for its request
                          const std::vector<uint8_t> payload = getPayload(echoResponse);
                          CAPTURE(payload);
                          REQUIRE(payload.size() == static_cast<size_t>(1));
                          REQUIRE(payload.at(0) < requestCount);
                          ++echoResponseCounts.at(payload.at(0));
                      });

        // check one echo plus one per timed out resend
        for (uint8_t identifier = 0; identifier < requestCount; ++identifier) {
            CAPTURE(identifier);
            REQUIRE(echoResponseCounts.at(identifier) >= static_cast<size_t>(1));
            REQUIRE(echoResponseCounts.at(identifier) <= timedOutResendCounts.at(identifier) + 1);
        }
    }
}

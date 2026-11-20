#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
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
    std::vector<Messages::MessageType> confirmedMessageTypes;                // confirmed requests
    std::vector<FailedRequest> failedRequests;                               // failed requests
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

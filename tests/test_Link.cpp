#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <catch2/catch_all.hpp>

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
    uint16_t messageCounter = 0; // message counter of the message
    uint16_t messageType = 0;    // message type of the message
    bool confirmed = false;      // message reached the peer
    Definitions::DeliveryFailureType deliveryFailureType
        = Definitions::DeliveryFailureType::UNKNOWN; // reason of a failed delivery
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
        , broken(false)
        , duplicated(false)
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
     * Send data into the queue unless the transport is broken or the filter drops the data.
     */
    [[nodiscard]] bool sendData(const uint8_t *data, const size_t size) final
    {
        // transport must not be broken
        if (this->broken) {
            return false;
        }

        // keep track of sent data
        ++this->sendCount;
        const std::vector<uint8_t> sentData(data, data + size);

        // drop the data the filter rejects
        if (this->filter != nullptr && !this->filter(this->sendCount, sentData)) {
            return true;
        }

        // queue the data once or twice
        this->sentDataQueue.push_back(sentData);
        if (this->duplicated) {
            this->sentDataQueue.push_back(sentData);
        }

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

    /*
     * Set whether the transport fails every send.
     */
    void setBroken(const bool broken)
    {
        // set whether the transport is broken
        this->broken = broken;
    }

    /*
     * Set whether the transport queues every sent data twice.
     */
    void setDuplicated(const bool duplicated)
    {
        // set whether the sent data is duplicated
        this->duplicated = duplicated;
    }

private:
    std::deque<std::vector<uint8_t>> sentDataQueue; // sent data that waits for the receiver
    DataFilter filter;                              // filter that keeps or drops sent data
    size_t sendCount;                               // number of sends so far
    bool broken;                                    // transport fails every send
    bool duplicated;                                // transport queues every sent data twice
};

class MockLinkHandler final : public Link::LinkHandler
{
public:
    /*
     * Mock link handler that records every link event and answers with a set message status.
     */
    MockLinkHandler()
        : messageStatusType(Definitions::MessageStatusType::ACCEPTED)
    {}
    ~MockLinkHandler() final {}

    // delete copy constructor and copy assignment operator
    MockLinkHandler(const MockLinkHandler &) = delete;
    MockLinkHandler &operator=(const MockLinkHandler &) = delete;

    // delete move constructor and move assignment operator
    MockLinkHandler(MockLinkHandler &&) = delete;
    MockLinkHandler &operator=(MockLinkHandler &&) = delete;

public:
    /*
     * Listen for a received message, record it and get the set message status.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame) final
    {
        // record the message
        this->messages.push_back(frame);

        // expose the set message status
        return this->messageStatusType;
    }

    /*
     * Listen for a confirmed delivery and record it.
     */
    void onDeliveryConfirmed(const uint16_t messageCounter, const uint16_t messageType) final
    {
        // record the confirmed delivery
        DeliveryResult deliveryResult;
        deliveryResult.messageCounter = messageCounter;
        deliveryResult.messageType = messageType;
        deliveryResult.confirmed = true;
        this->deliveryResults.push_back(deliveryResult);
    }

    /*
     * Listen for a failed delivery and record it.
     */
    void onDeliveryFailed(const uint16_t messageCounter,
                          const uint16_t messageType,
                          const Definitions::DeliveryFailureType deliveryFailureType,
                          const Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // record the failed delivery
        DeliveryResult deliveryResult;
        deliveryResult.messageCounter = messageCounter;
        deliveryResult.messageType = messageType;
        deliveryResult.confirmed = false;
        deliveryResult.deliveryFailureType = deliveryFailureType;
        deliveryResult.remoteMessageStatusType = remoteMessageStatusType;
        this->deliveryResults.push_back(deliveryResult);
    }

public:
    /*
     * Get the received messages in arrival order.
     */
    [[nodiscard]] std::vector<Frame::Frame> getMessages() const
    {
        // expose the received messages
        return this->messages;
    }

    /*
     * Get the delivery results in arrival order.
     */
    [[nodiscard]] std::vector<DeliveryResult> getDeliveryResults() const
    {
        // expose the delivery results
        return this->deliveryResults;
    }

    /*
     * Set the message status that answers a received message.
     */
    void setMessageStatusType(const Definitions::MessageStatusType messageStatusType)
    {
        // set the message status
        this->messageStatusType = messageStatusType;
    }

private:
    std::vector<Frame::Frame> messages;               // received messages
    std::vector<DeliveryResult> deliveryResults;      // finished deliveries
    Definitions::MessageStatusType messageStatusType; // answer to a received message
};

} // namespace

class TestLink
{
public:
    /*
     * Link test fixture.
     */
    TestLink()
        : messageCounter(0)
    {
        // initialize clock
        static constexpr std::chrono::milliseconds startTime = 1000ms;
        this->clock.setTime(startTime);

        // initialize links
        this->testLink.reset(new Link::Link(this->testTransport,
                                            this->testLinkHandler,
                                            this->clock,
                                            this->getLinkConfiguration()));
        this->anotherLink.reset(new Link::Link(this->anotherTransport,
                                               this->anotherLinkHandler,
                                               this->clock,
                                               this->getLinkConfiguration()));
    }
    ~TestLink()
    {
        // close links
        this->anotherLink.reset(nullptr);
        this->testLink.reset(nullptr);
    }

    /*
     * Get the link configuration of both links.
     */
    [[nodiscard]] static Link::LinkConfiguration getLinkConfiguration()
    {
        // shorten the retransmissions of the default link configuration
        static constexpr std::chrono::milliseconds retransmitTimeout = 100ms;
        static constexpr uint8_t maximumRetransmissions = 3;
        Link::LinkConfiguration linkConfiguration;
        linkConfiguration.retransmitTimeout = retransmitTimeout;
        linkConfiguration.maximumRetransmissions = maximumRetransmissions;
        linkConfiguration.interByteTimeout = Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC;

        // expose the link configuration
        return linkConfiguration;
    }

    /*
     * Get the bytes of a frame.
     */
    [[nodiscard]] static std::vector<uint8_t> getFrameData(const Frame::Frame &frame)
    {
        // expose a copy of the frame bytes
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
    }

    /*
     * Get the payload bytes of a frame.
     */
    [[nodiscard]] static std::vector<uint8_t> getPayload(const Frame::Frame &frame)
    {
        // expose a copy of the payload bytes
        return std::vector<uint8_t>(frame.getPayload(),
                                    frame.getPayload() + frame.getPayloadLength());
    }

    /*
     * Deliver the sent data in both directions until no data waits.
     */
    void pump()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->testTransport.getEmpty() && this->anotherTransport.getEmpty()) {
                return;
            }

            // deliver the data of the test link to another link
            std::vector<uint8_t> data;
            while (this->testTransport.receiveData(data)) {
                this->anotherLink->onReceivedData(data.data(), data.size());
            }

            // deliver the data of another link to the test link
            while (this->anotherTransport.receiveData(data)) {
                this->testLink->onReceivedData(data.data(), data.size());
            }
        }
    }

    /*
     * Let time pass, maintain both links and deliver what they sent.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // let time pass
        this->clock.advanceTime(duration);

        // maintain both links
        this->testLink->maintain();
        this->anotherLink->maintain();

        // deliver the sent data
        this->pump();
    }

    /*
     * Get the bytes of a ping request with the counter of the golden frame and its retransmission.
     * The NACK of the retransmission and the ACK of a ping request of the peer follow them.
     */
    [[nodiscard]] std::vector<std::vector<uint8_t>> getGoldenFrameBytes(
        Link::Link &link,
        MockTransport &transport,
        Link::Link &peerLink,
        MockTransport &peerTransport,
        MockLinkHandler &peerLinkHandler)
    {
        // send fire and forget messages into nowhere until both links reach the golden counter
        static constexpr uint16_t goldenMessageCounter = 7;
        std::vector<uint8_t> data;
        while (link.getNextMessageCounter() < goldenMessageCounter) {
            REQUIRE(link.sendMessage(0x0100, nullptr, 0, false, this->messageCounter)
                    == Definitions::SendStatusType::SENT);
            REQUIRE(peerLink.sendMessage(0x0100, nullptr, 0, false, this->messageCounter)
                    == Definitions::SendStatusType::SENT);
        }
        while (transport.receiveData(data) || peerTransport.receiveData(data)) {
            data.clear();
        }

        // send the ping request of the golden frame and its retransmission
        const std::vector<uint8_t> pingPayload = {0xE8, 0x03, 0x00, 0x00};
        std::vector<std::vector<uint8_t>> frameBytes;
        REQUIRE(link.sendMessage(0x0001,
                                 pingPayload.data(),
                                 pingPayload.size(),
                                 true,
                                 this->messageCounter)
                == Definitions::SendStatusType::SENT);
        REQUIRE(transport.receiveData(data));
        frameBytes.push_back(data);
        this->clock.advanceTime(getLinkConfiguration().retransmitTimeout);
        link.maintain();
        REQUIRE(transport.receiveData(data));
        frameBytes.push_back(data);

        // reject the retransmission as busy
        peerLinkHandler.setMessageStatusType(Definitions::MessageStatusType::BUSY);
        peerLink.onReceivedData(data.data(), data.size());
        REQUIRE(peerTransport.receiveData(data));
        frameBytes.push_back(data);

        // acknowledge the ping request of the peer
        peerLinkHandler.setMessageStatusType(Definitions::MessageStatusType::ACCEPTED);
        REQUIRE(peerLink.sendMessage(0x0001,
                                     pingPayload.data(),
                                     pingPayload.size(),
                                     true,
                                     this->messageCounter)
                == Definitions::SendStatusType::SENT);
        REQUIRE(peerTransport.receiveData(data));
        link.onReceivedData(data.data(), data.size());
        REQUIRE(transport.receiveData(data));
        frameBytes.push_back(data);

        // expose the bytes of the ping request, the retransmission, the NACK and the ACK
        return frameBytes;
    }

    /*
     * Replace both links by route capable links.
     */
    void makeRouteCapable()
    {
        // close links
        this->anotherLink.reset(nullptr);
        this->testLink.reset(nullptr);

        // initialize route capable links
        Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
        linkConfiguration.routeCapable = true;
        this->testLink.reset(new Link::Link(this->testTransport,
                                            this->testLinkHandler,
                                            this->clock,
                                            linkConfiguration));
        this->anotherLink.reset(new Link::Link(this->anotherTransport,
                                               this->anotherLinkHandler,
                                               this->clock,
                                               linkConfiguration));
    }

    /*
     * Get the route header from the host to node 3.
     */
    [[nodiscard]] static Frame::RouteHeader getRouteHeader()
    {
        // create route header from the host to node 3
        static constexpr uint8_t destinationAddress = 3;
        Frame::RouteHeader routeHeader;
        routeHeader.sourceAddress = Definitions::HOST_ADDRESS;
        routeHeader.destinationAddress = destinationAddress;
        routeHeader.timeToLive = Settings::DEFAULT_TIME_TO_LIVE;

        // expose the route header
        return routeHeader;
    }

    /*
     * Send an acknowledged message from the test link and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendAcknowledgedMessage(
        const uint16_t messageType, const std::vector<uint8_t> &payload)
    {
        // expose the send status of the message
        return this->testLink->sendMessage(messageType,
                                           payload.data(),
                                           payload.size(),
                                           true,
                                           this->messageCounter);
    }

    // delete copy and move operators
    TestLink(const TestLink &other) = delete;
    TestLink &operator=(const TestLink &other) = delete;
    TestLink(TestLink &&other) noexcept = delete;
    TestLink &operator=(TestLink &&other) noexcept = delete;

protected:
    MockClock clock;                         // clock of both links
    MockTransport testTransport;             // data sent by the test link
    MockTransport anotherTransport;          // data sent by another link
    MockLinkHandler testLinkHandler;         // events of the test link
    MockLinkHandler anotherLinkHandler;      // events of another link
    std::unique_ptr<Link::Link> testLink;    // sender in most tests
    std::unique_ptr<Link::Link> anotherLink; // receiver in most tests
    uint16_t messageCounter;                 // message counter of the latest sent message
};

TEST_CASE_METHOD(TestLink,
                 "An unacknowledged message is delivered and not answered",
                 "[Link][testUnacknowledgedMessage]")
{
    // send an unacknowledged message
    const std::vector<uint8_t> payload = {1, 2, 3};
    REQUIRE(testLink->sendMessage(0x0103, payload.data(), payload.size(), false, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the message on both links
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(getPayload(messages.at(0)) == payload);
    REQUIRE_FALSE(Definitions::isFrameFlagSet(messages.at(0).getFlags(),
                                              Definitions::FrameFlagType::ACK_REQUEST));
    REQUIRE(anotherLink->getStatistics().outgoingAcknowledgementCounter == static_cast<uint64_t>(0));
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestLink,
                 "An acknowledged message is confirmed by the peer",
                 "[Link][testAcknowledgedMessage]")
{
    // send an acknowledged message over a stale message counter
    messageCounter = std::numeric_limits<uint16_t>::max();
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x01}))
            == Definitions::SendStatusType::SENT);
    REQUIRE(messageCounter == 0);
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(1));
    pump();

    // check the confirmation on both links
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).confirmed);
    REQUIRE(deliveryResults.at(0).messageCounter == 0);
    REQUIRE(deliveryResults.at(0).messageType == 0x0100);
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
    REQUIRE(anotherLink->getStatistics().outgoingAcknowledgementCounter == static_cast<uint64_t>(1));
    REQUIRE(testLink->getStatistics().incomingAcknowledgementCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestLink,
                 "Acknowledgements are never delivered to the application",
                 "[Link][testAcknowledgementNotDelivered]")
{
    // send an acknowledged message
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::SENT);
    pump();

    // check that the acknowledgement stays in the link
    REQUIRE(testLinkHandler.getMessages().empty());
    REQUIRE(testLink->getStatistics().incomingFrameCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestLink,
                 "Message counters increase by one and wrap from 65535 to 0",
                 "[Link][testMessageCounterWrap]")
{
    // send all but the last message counter into nowhere
    static constexpr uint32_t messageCount = std::numeric_limits<uint16_t>::max();
    testTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });
    for (uint32_t messageIndex = 0; messageIndex < messageCount; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(testLink->sendMessage(1, nullptr, 0, false, messageCounter)
                == Definitions::SendStatusType::SENT);
    }
    REQUIRE(testLink->getNextMessageCounter() == std::numeric_limits<uint16_t>::max());

    // send the last message counter
    REQUIRE(testLink->sendMessage(1, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::SENT);
    REQUIRE(messageCounter == std::numeric_limits<uint16_t>::max());
    REQUIRE(testLink->getNextMessageCounter() == 0);
}

TEST_CASE_METHOD(TestLink,
                 "A lost data frame is retransmitted with the retransmission flag",
                 "[Link][testRetransmission]")
{
    // send an acknowledged message whose first transmission is lost
    testTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0xAA}))
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(anotherLinkHandler.getMessages().empty());

    // let the retransmit timeout pass but its last millisecond
    static constexpr std::chrono::milliseconds lastMillisecond = 1ms;
    advance(getLinkConfiguration().retransmitTimeout - lastMillisecond);
    REQUIRE(anotherLinkHandler.getMessages().empty());

    // let the last millisecond pass
    advance(lastMillisecond);

    // check the retransmission on both links
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(Definitions::isFrameFlagSet(messages.at(0).getFlags(),
                                        Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).confirmed);
    REQUIRE(testLink->getStatistics().retransmissionCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(
    TestLink,
    "A lost acknowledgement causes a duplicate that is answered but not delivered again",
    "[Link][testLostAcknowledgement]")
{
    // send an acknowledged message whose first acknowledgement is lost
    anotherTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0xAA}))
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(testLinkHandler.getDeliveryResults().empty());

    // let the retransmit timeout pass
    advance(getLinkConfiguration().retransmitTimeout);

    // check the single delivery and confirmation and the counted duplicate
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).confirmed);
    REQUIRE(anotherLink->getStatistics().duplicateFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(anotherLink->getStatistics().outgoingAcknowledgementCounter == static_cast<uint64_t>(2));
}

TEST_CASE_METHOD(TestLink,
                 "Delivery fails after the configured number of retransmissions",
                 "[Link][testDeliveryTimeout]")
{
    // send an acknowledged message into a transport that drops everything
    testTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x01}))
            == Definitions::SendStatusType::SENT);

    // let every retransmission time out
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission < linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        advance(linkConfiguration.retransmitTimeout);
        REQUIRE(testLinkHandler.getDeliveryResults().empty());
    }

    // let the last retransmission time out
    advance(linkConfiguration.retransmitTimeout);

    // check the failed delivery
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(deliveryResults.at(0).confirmed);
    REQUIRE(deliveryResults.at(0).deliveryFailureType
            == Definitions::DeliveryFailureType::TIMED_OUT);
    REQUIRE(testLink->getStatistics().outgoingFrameCounter
            == static_cast<uint64_t>(linkConfiguration.maximumRetransmissions) + 1);
    REQUIRE(testLink->getStatistics().failedDeliveryCounter == static_cast<uint64_t>(1));
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
}

TEST_CASE_METHOD(TestLink,
                 "A rejected message reports the remote status and is not retransmitted",
                 "[Link][testRejectedMessage]")
{
    // send an acknowledged message that another link rejects
    anotherLinkHandler.setMessageStatusType(
        Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE);
    REQUIRE(sendAcknowledgedMessage(0x7777, std::vector<uint8_t>())
            == Definitions::SendStatusType::SENT);
    pump();

    // check the rejection
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(deliveryResults.at(0).confirmed);
    REQUIRE(deliveryResults.at(0).deliveryFailureType == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType
            == Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE);

    // check that no retransmission follows
    advance(10 * getLinkConfiguration().retransmitTimeout);
    REQUIRE(testLink->getStatistics().retransmissionCounter == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(
    TestLink,
    "A lost rejection is repeated with the same status without asking the application again",
    "[Link][testLostRejection]")
{
    // send an acknowledged message whose first rejection is lost
    anotherLinkHandler.setMessageStatusType(Definitions::MessageStatusType::BUSY);
    anotherTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::SENT);
    pump();

    // let the retransmit timeout pass
    advance(getLinkConfiguration().retransmitTimeout);

    // check one delivery and the repeated rejection
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType == Definitions::MessageStatusType::BUSY);
}

TEST_CASE_METHOD(TestLink,
                 "The window limits acknowledged messages but not fire and forget ones",
                 "[Link][testWindowLimit]")
{
    // fill the window with acknowledged messages
    for (size_t messageIndex = 0; messageIndex < Settings::MaximumPendingFrames; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
                == Definitions::SendStatusType::SENT);
    }

    // check that only unacknowledged messages can be sent
    REQUIRE_FALSE(testLink->getSendAcknowledgedAllowed());
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::WINDOW_FULL);
    REQUIRE(testLink->sendMessage(0x0103, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::SENT);

    // check that the answers free the window again
    pump();
    REQUIRE(testLink->getSendAcknowledgedAllowed());
    REQUIRE(testLinkHandler.getDeliveryResults().size() == Settings::MaximumPendingFrames);
}

TEST_CASE_METHOD(TestLink,
                 "An oversized payload is refused without consuming a message counter",
                 "[Link][testOversizedPayload]")
{
    // send a payload above the limit
    const std::vector<uint8_t> payload(Settings::MaximumPayloadSize + 1, 0);
    REQUIRE(sendAcknowledgedMessage(0x0100, payload)
            == Definitions::SendStatusType::PAYLOAD_TOO_LONG);

    // check the unused message counter
    REQUIRE(testLink->getNextMessageCounter() == 0);
}

TEST_CASE_METHOD(TestLink,
                 "A missing payload is refused without consuming a message counter",
                 "[Link][testMissingPayload]")
{
    // send a payload length without its payload
    static constexpr size_t payloadLength = 1;
    REQUIRE(testLink->sendMessage(0x0100, nullptr, payloadLength, true, messageCounter)
            == Definitions::SendStatusType::INVALID_ARGUMENT);

    // check that nothing left the test link
    REQUIRE(testLink->getNextMessageCounter() == 0);
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
    REQUIRE(testTransport.getEmpty());
}

TEST_CASE_METHOD(TestLink,
                 "A broken transport is reported and leaves the window empty",
                 "[Link][testBrokenTransport]")
{
    // send an acknowledged message into a broken transport
    testTransport.setBroken(true);
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::TRANSPORT_ERROR);

    // check the empty window and the statistics
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
    REQUIRE(testLink->getStatistics().transportErrorCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestLink,
                 "A restarted peer reusing message counters is not mistaken for a duplicate",
                 "[Link][testRestartedPeer]")
{
    // send four acknowledged messages
    static constexpr uint8_t messageCount = 4;
    for (uint8_t messageIndex = 0; messageIndex < messageCount; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({messageIndex}))
                == Definitions::SendStatusType::SENT);
        pump();
    }
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(messageCount));

    // close the test link
    testLink.reset(nullptr);

    // restart the test link with its message counters at zero
    testLink.reset(new Link::Link(testTransport, testLinkHandler, clock, getLinkConfiguration()));
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x55}))
            == Definitions::SendStatusType::SENT);
    pump();

    // check the new message (only retransmissions are duplicates)
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(messageCount) + 1);
    REQUIRE(getPayload(messages.back()) == std::vector<uint8_t>({0x55}));
    REQUIRE(anotherLink->getStatistics().duplicateFrameCounter == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestLink,
                 "A late duplicate acknowledgement is counted and ignored",
                 "[Link][testDuplicateAcknowledgement]")
{
    // send an acknowledged message whose acknowledgement arrives twice
    anotherTransport.setDuplicated(true);
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::SENT);
    pump();

    // check one confirmation and the unexpected acknowledgement
    REQUIRE(testLinkHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(testLink->getStatistics().unexpectedAcknowledgementCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestLink,
                 "Retransmission timing keeps working beyond 32 bit milliseconds",
                 "[Link][testRetransmissionBeyond32Bit]")
{
    // send a lost acknowledged message 64 milliseconds before 2^32 milliseconds
    static constexpr std::chrono::milliseconds wrapTimestamp
        = std::chrono::milliseconds(std::numeric_limits<uint32_t>::max()) + 1ms;
    clock.setTime(wrapTimestamp - 64ms);
    testTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::SENT);
    pump();

    // step over 2^32 milliseconds one millisecond at a time
    static constexpr std::chrono::milliseconds timeStep = 1ms;
    const int64_t stepCount = getLinkConfiguration().retransmitTimeout / timeStep;
    for (int64_t step = 1; step < stepCount; ++step) {
        CAPTURE(step);
        advance(timeStep);
        REQUIRE(anotherLinkHandler.getMessages().empty());
    }

    // let the last millisecond of the retransmit timeout pass
    advance(timeStep);
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(clock.getTime() > wrapTimestamp);
}

TEST_CASE_METHOD(TestLink,
                 "Several messages in flight are matched to their own acknowledgements",
                 "[Link][testMessagesInFlight]")
{
    // send three acknowledged messages whose second data frame is lost
    static constexpr uint8_t messageCount = 3;
    testTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 2;
        });
    std::vector<uint16_t> messageCounters;
    for (uint8_t messageIndex = 0; messageIndex < messageCount; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({messageIndex}))
                == Definitions::SendStatusType::SENT);
        messageCounters.push_back(messageCounter);
    }
    pump();
    REQUIRE(testLinkHandler.getDeliveryResults().size() == static_cast<size_t>(2));
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(1));

    // let the retransmit timeout pass
    advance(getLinkConfiguration().retransmitTimeout);

    // check that the retransmission confirms the second message
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(messageCount));
    REQUIRE(deliveryResults.back().messageCounter == messageCounters.at(1));
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(messageCount));
}

TEST_CASE_METHOD(TestLink,
                 "A sender never outruns the duplicate history of its receiver",
                 "[Link][testAcknowledgedSpread]")
{
    // send a first acknowledged message whose every frame is lost
    testTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount, const std::vector<uint8_t> &data) noexcept {
            return Frame::Frame(data.data(), data.size()).getMessageCounter() != 0;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x00}))
            == Definitions::SendStatusType::SENT);

    // send acknowledged messages until the spread limit stops them
    size_t sentCount = 1;
    while (testLink->getSendAcknowledgedAllowed()) {
        // send the next acknowledged message and count it
        CAPTURE(sentCount);
        REQUIRE(
            sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({static_cast<uint8_t>(sentCount)}))
            == Definitions::SendStatusType::SENT);
        pump();
        ++sentCount;
    }
    REQUIRE(sentCount == Settings::MAXIMUM_ACKNOWLEDGED_SPREAD);
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>())
            == Definitions::SendStatusType::WINDOW_FULL);

    // send an unacknowledged message (never limited)
    REQUIRE(testLink->sendMessage(0x0103, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::SENT);

    // let the lost message fail
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission <= linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        advance(linkConfiguration.retransmitTimeout);
    }

    // check that acknowledged messages can be sent again
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE_FALSE(deliveryResults.empty());
    REQUIRE(deliveryResults.back().deliveryFailureType
            == Definitions::DeliveryFailureType::TIMED_OUT);
    REQUIRE(testLink->getSendAcknowledgedAllowed());
}

TEST_CASE_METHOD(TestLink,
                 "A late retransmission is recognized even after many newer messages",
                 "[Link][testLateRetransmission]")
{
    // send a first acknowledged message whose first acknowledgement is lost
    anotherTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x00}))
            == Definitions::SendStatusType::SENT);
    pump();

    // send newer acknowledged messages until the spread limit stops them
    while (testLink->getSendAcknowledgedAllowed()) {
        REQUIRE(sendAcknowledgedMessage(0x0100, std::vector<uint8_t>({0x01}))
                == Definitions::SendStatusType::SENT);
        pump();
    }

    // let the retransmit timeout pass
    advance(getLinkConfiguration().retransmitTimeout);

    // check that the first message was delivered once
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    const size_t firstMessageCount = static_cast<size_t>(
        std::count_if(messages.cbegin(), messages.cend(), [](const Frame::Frame &message) {
            return message.getMessageCounter() == 0;
        }));
    REQUIRE(firstMessageCount == static_cast<size_t>(1));
    REQUIRE(anotherLink->getStatistics().duplicateFrameCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(TestLink,
                 "A link that is not route capable sends the bytes of version 1",
                 "[Link][testVersion1Bytes]")
{
    // get the ping request, its retransmission, a NACK and an ACK of two links without routes
    const std::vector<std::vector<uint8_t>> frameBytes = getGoldenFrameBytes(*testLink,
                                                                             testTransport,
                                                                             *anotherLink,
                                                                             anotherTransport,
                                                                             anotherLinkHandler);

    // check the bytes of version 1 (CRC32 checked with Python binascii)
    static const std::vector<uint8_t> pingRequestBytes = {0xA5,
                                                          0xC3,
                                                          0x01,
                                                          0x01,
                                                          0x07,
                                                          0x00,
                                                          0x01,
                                                          0x00,
                                                          0x04,
                                                          0xE8,
                                                          0x03,
                                                          0x00,
                                                          0x00,
                                                          0x21,
                                                          0x12,
                                                          0xD0,
                                                          0x63};
    static const std::vector<uint8_t> retransmissionBytes = {0xA5,
                                                             0xC3,
                                                             0x01,
                                                             0x09,
                                                             0x07,
                                                             0x00,
                                                             0x01,
                                                             0x00,
                                                             0x04,
                                                             0xE8,
                                                             0x03,
                                                             0x00,
                                                             0x00,
                                                             0xD5,
                                                             0x52,
                                                             0x07,
                                                             0x70};
    static const std::vector<uint8_t> rejectionBytes
        = {0xA5, 0xC3, 0x01, 0x04, 0x07, 0x00, 0x01, 0x00, 0x01, 0x04, 0x61, 0x59, 0x08, 0xE6};
    static const std::vector<uint8_t> acknowledgementBytes
        = {0xA5, 0xC3, 0x01, 0x02, 0x07, 0x00, 0x01, 0x00, 0x00, 0xE6, 0xC3, 0x31, 0xC5};
    CAPTURE(frameBytes);
    REQUIRE(frameBytes.size() == static_cast<size_t>(4));
    REQUIRE(frameBytes.at(0) == pingRequestBytes);
    REQUIRE(frameBytes.at(1) == retransmissionBytes);
    REQUIRE(frameBytes.at(2) == rejectionBytes);
    REQUIRE(frameBytes.at(3) == acknowledgementBytes);
}

TEST_CASE_METHOD(TestLink,
                 "A route capable link sets the route capable flag on every frame it sends",
                 "[Link][testRouteCapableFlag]")
{
    // get the ping request, its retransmission, a NACK and an ACK of two route capable links
    Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    linkConfiguration.routeCapable = true;
    Link::Link testRoutingLink(testTransport, testLinkHandler, clock, linkConfiguration);
    Link::Link anotherRoutingLink(anotherTransport, anotherLinkHandler, clock, linkConfiguration);
    const std::vector<std::vector<uint8_t>> frameBytes = getGoldenFrameBytes(testRoutingLink,
                                                                             testTransport,
                                                                             anotherRoutingLink,
                                                                             anotherTransport,
                                                                             anotherLinkHandler);

    // check the direct frames with the route capable flag (CRC32 checked with Python binascii)
    static const std::vector<uint8_t> pingRequestBytes = {0xA5,
                                                          0xC3,
                                                          0x01,
                                                          0x11,
                                                          0x07,
                                                          0x00,
                                                          0x01,
                                                          0x00,
                                                          0x04,
                                                          0xE8,
                                                          0x03,
                                                          0x00,
                                                          0x00,
                                                          0xC9,
                                                          0x93,
                                                          0x7E,
                                                          0x44};
    static const std::vector<uint8_t> retransmissionBytes = {0xA5,
                                                             0xC3,
                                                             0x01,
                                                             0x19,
                                                             0x07,
                                                             0x00,
                                                             0x01,
                                                             0x00,
                                                             0x04,
                                                             0xE8,
                                                             0x03,
                                                             0x00,
                                                             0x00,
                                                             0x3D,
                                                             0xD3,
                                                             0xA9,
                                                             0x57};
    static const std::vector<uint8_t> rejectionBytes
        = {0xA5, 0xC3, 0x01, 0x14, 0x07, 0x00, 0x01, 0x00, 0x01, 0x04, 0xAF, 0xC5, 0xD6, 0x81};
    static const std::vector<uint8_t> acknowledgementBytes
        = {0xA5, 0xC3, 0x01, 0x12, 0x07, 0x00, 0x01, 0x00, 0x00, 0x7D, 0xC1, 0xE7, 0xC6};
    CAPTURE(frameBytes);
    REQUIRE(frameBytes.size() == static_cast<size_t>(4));
    REQUIRE(frameBytes.at(0) == pingRequestBytes);
    REQUIRE(frameBytes.at(1) == retransmissionBytes);
    REQUIRE(frameBytes.at(2) == rejectionBytes);
    REQUIRE(frameBytes.at(3) == acknowledgementBytes);
}

TEST_CASE_METHOD(TestLink,
                 "The peer version follows every valid frame of the peer",
                 "[Link][testPeerVersion]")
{
    // check the peer version before the first frame
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::UNKNOWN);

    // receive frames of a peer that changes its version and check the learned peer version
    static constexpr uint8_t routeCapableFlags = 0x10;
    Frame::RouteHeader routeHeader;
    routeHeader.sourceAddress = Definitions::HOST_ADDRESS;
    routeHeader.destinationAddress = Definitions::MINIMUM_NODE_ADDRESS;
    routeHeader.timeToLive = Settings::DEFAULT_TIME_TO_LIVE;
    const std::vector<std::pair<Frame::Frame, Definitions::PeerVersionType>> receivedFrames = {
        {Frame::Frame(0, 1, 0x0100, nullptr, 0), Definitions::PeerVersionType::VERSION_1},
        {Frame::Frame(routeCapableFlags, 2, 0x0100, nullptr, 0),
         Definitions::PeerVersionType::VERSION_2},
        {Frame::Frame(0, 3, 0x0100, nullptr, 0), Definitions::PeerVersionType::VERSION_1},
        {Frame::Frame(0, 4, 0x0100, routeHeader, nullptr, 0),
         Definitions::PeerVersionType::VERSION_2},
        {Frame::Frame(0x02, 5, 0x0100, nullptr, 0), Definitions::PeerVersionType::VERSION_1},
    };
    std::for_each(receivedFrames.cbegin(),
                  receivedFrames.cend(),
                  [this](
                      const std::pair<Frame::Frame, Definitions::PeerVersionType> &receivedFrame) {
                      // receive the frame
                      CAPTURE(receivedFrame.first.getMessageCounter());
                      testLink->onReceivedData(receivedFrame.first.getData(),
                                               receivedFrame.first.getSize());

                      // check the learned peer version
                      REQUIRE(testLink->getPeerVersionType() == receivedFrame.second);
                  });

    // check that a corrupted frame with the flag leaves the peer version
    std::vector<uint8_t> corruptedData = getFrameData(
        Frame::Frame(routeCapableFlags, 6, 0x0100, nullptr, 0));
    corruptedData.back() = static_cast<uint8_t>(corruptedData.back() ^ 0x01);
    testLink->onReceivedData(corruptedData.data(), corruptedData.size());
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::VERSION_1);
}

TEST_CASE_METHOD(TestLink,
                 "A routed message is delivered with its route and answered by a direct ACK",
                 "[Link][testRoutedMessage]")
{
    // send an acknowledged routed message from a route capable link
    makeRouteCapable();
    const std::vector<uint8_t> payload = {0xE8, 0x03, 0x00, 0x00};
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(),
                                        0x0001,
                                        payload.data(),
                                        payload.size(),
                                        true,
                                        messageCounter)
            == Definitions::SendStatusType::SENT);
    REQUIRE(messageCounter == 0);

    // check the routed frame on the wire and keep the answer of another link
    std::vector<uint8_t> data;
    REQUIRE(testTransport.receiveData(data));
    const Frame::Frame sentFrame(data.data(), data.size());
    REQUIRE(sentFrame.getRouted());
    REQUIRE(sentFrame.getFlag(Definitions::FrameFlagType::ROUTE_CAPABLE));
    anotherLink->onReceivedData(data.data(), data.size());
    REQUIRE(anotherTransport.receiveData(data));
    testLink->onReceivedData(data.data(), data.size());

    // check the delivered message with its route
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(messages.at(0).getSourceAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(messages.at(0).getDestinationAddress() == 3);
    REQUIRE(messages.at(0).getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(getPayload(messages.at(0)) == payload);

    // check the direct ACK and the confirmed delivery
    const Frame::Frame acknowledgementFrame(data.data(), data.size());
    REQUIRE(acknowledgementFrame.getVersion() == Definitions::PROTOCOL_VERSION);
    REQUIRE(acknowledgementFrame.getFlag(Definitions::FrameFlagType::ACK));
    REQUIRE(acknowledgementFrame.getFlag(Definitions::FrameFlagType::ROUTE_CAPABLE));
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).confirmed);
    REQUIRE(anotherLink->getPeerVersionType() == Definitions::PeerVersionType::VERSION_2);
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::VERSION_2);
}

TEST_CASE_METHOD(
    TestLink,
    "A routed message is retransmitted as the same routed frame and kept from duplicates",
    "[Link][testRoutedRetransmission]")
{
    // send an acknowledged routed message whose first transmission and first ACK are lost
    makeRouteCapable();
    testTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    anotherTransport.setFilter(
        [](const size_t sendCount, [[maybe_unused]] const std::vector<uint8_t> &data) noexcept {
            return sendCount != 1;
        });
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(), 0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // let two retransmit timeouts pass
    advance(getLinkConfiguration().retransmitTimeout);
    advance(getLinkConfiguration().retransmitTimeout);

    // check the single delivery of the retransmitted routed frame and the answered duplicate
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(messages.at(0).getRouted());
    REQUIRE(messages.at(0).getFlag(Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(messages.at(0).getDestinationAddress() == 3);
    REQUIRE(messages.at(0).getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(anotherLink->getStatistics().duplicateFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(testLinkHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(testLinkHandler.getDeliveryResults().at(0).confirmed);
    REQUIRE(testLink->getStatistics().retransmissionCounter == static_cast<uint64_t>(2));
}

TEST_CASE_METHOD(TestLink,
                 "A routed message needs a route capable link, both addresses and a time to live",
                 "[Link][testRoutedMessageRefused]")
{
    // check the routed message on a link that is not route capable
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(), 0x0100, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::INVALID_ARGUMENT);

    // check the route headers without an address or without a time to live
    makeRouteCapable();
    Frame::RouteHeader withoutSourceAddress = getRouteHeader();
    withoutSourceAddress.sourceAddress = Definitions::UNASSIGNED_ADDRESS;
    Frame::RouteHeader withoutDestinationAddress = getRouteHeader();
    withoutDestinationAddress.destinationAddress = Definitions::UNASSIGNED_ADDRESS;
    Frame::RouteHeader withoutTimeToLive = getRouteHeader();
    withoutTimeToLive.timeToLive = 0;
    REQUIRE(
        testLink->sendRoutedMessage(withoutSourceAddress, 0x0100, nullptr, 0, false, messageCounter)
        == Definitions::SendStatusType::INVALID_ARGUMENT);
    REQUIRE(testLink->sendRoutedMessage(withoutDestinationAddress,
                                        0x0100,
                                        nullptr,
                                        0,
                                        false,
                                        messageCounter)
            == Definitions::SendStatusType::INVALID_ARGUMENT);
    REQUIRE(testLink->sendRoutedMessage(withoutTimeToLive, 0x0100, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::INVALID_ARGUMENT);

    // check that no frame was sent and no message counter was used
    REQUIRE(testTransport.getEmpty());
    REQUIRE(testLink->getNextMessageCounter() == 0);
}

TEST_CASE_METHOD(TestLink,
                 "A routed message that timed out makes the peer version unknown again",
                 "[Link][testRoutedMessageTimeout]")
{
    // learn the version of a route capable peer
    makeRouteCapable();
    REQUIRE(anotherLink->sendMessage(0x0100, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::VERSION_2);

    // send a direct and a routed message into a transport that drops everything
    testTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });
    REQUIRE(testLink->sendMessage(0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission <= linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        advance(linkConfiguration.retransmitTimeout);
    }

    // check that the timed out direct message leaves the peer version
    REQUIRE(testLinkHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::VERSION_2);

    // let a routed message time out
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(), 0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    for (uint8_t retransmission = 0; retransmission <= linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        advance(linkConfiguration.retransmitTimeout);
    }

    // check the unknown peer version after the timed out routed message
    REQUIRE(testLinkHandler.getDeliveryResults().size() == static_cast<size_t>(2));
    REQUIRE(testLinkHandler.getDeliveryResults().at(1).deliveryFailureType
            == Definitions::DeliveryFailureType::TIMED_OUT);
    REQUIRE(testLink->getPeerVersionType() == Definitions::PeerVersionType::UNKNOWN);
}

TEST_CASE_METHOD(TestLink,
                 "A routed message refused as busy is retransmitted and delivered again",
                 "[Link][testRoutedBusyMessage]")
{
    // send an acknowledged routed message that another link refuses as busy
    makeRouteCapable();
    anotherLinkHandler.setMessageStatusType(Definitions::MessageStatusType::BUSY);
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(), 0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check that the test link keeps the message after the busy answer
    REQUIRE(anotherLinkHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(testLinkHandler.getDeliveryResults().empty());
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(1));

    // let another link take the retransmission one retransmit timeout later
    anotherLinkHandler.setMessageStatusType(Definitions::MessageStatusType::ACCEPTED);
    advance(getLinkConfiguration().retransmitTimeout);

    // check the second delivery of the retransmission and the confirmed delivery
    const std::vector<Frame::Frame> messages = anotherLinkHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(2));
    REQUIRE(messages.at(1).getFlag(Definitions::FrameFlagType::RETRANSMISSION));
    REQUIRE(anotherLink->getStatistics().duplicateFrameCounter == static_cast<uint64_t>(0));
    const std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).confirmed);
    REQUIRE(testLink->getStatistics().incomingRejectionCounter == static_cast<uint64_t>(1));
    REQUIRE(testLink->getStatistics().failedDeliveryCounter == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(
    TestLink,
    "A busy answer ends a routed delivery after the last retransmission, a broadcast at once",
    "[Link][testRoutedBusyLimit]")
{
    // send an acknowledged routed message that another link always refuses as busy
    makeRouteCapable();
    anotherLinkHandler.setMessageStatusType(Definitions::MessageStatusType::BUSY);
    REQUIRE(testLink->sendRoutedMessage(getRouteHeader(), 0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission < linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        REQUIRE(testLinkHandler.getDeliveryResults().empty());
        advance(linkConfiguration.retransmitTimeout);
    }

    // check the busy failure after the last retransmission
    REQUIRE(anotherLinkHandler.getMessages().size()
            == static_cast<size_t>(linkConfiguration.maximumRetransmissions) + 1);
    std::vector<DeliveryResult> deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).deliveryFailureType == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType == Definitions::MessageStatusType::BUSY);

    // send a routed broadcast and a direct message that another link refuses as busy
    Frame::RouteHeader broadcastRouteHeader = getRouteHeader();
    broadcastRouteHeader.destinationAddress = Definitions::BROADCAST_ADDRESS;
    REQUIRE(
        testLink->sendRoutedMessage(broadcastRouteHeader, 0x0100, nullptr, 0, true, messageCounter)
        == Definitions::SendStatusType::SENT);
    REQUIRE(testLink->sendMessage(0x0100, nullptr, 0, true, messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check that both deliveries end at the first busy answer
    deliveryResults = testLinkHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == static_cast<size_t>(3));
    REQUIRE(deliveryResults.at(1).remoteMessageStatusType == Definitions::MessageStatusType::BUSY);
    REQUIRE(deliveryResults.at(2).remoteMessageStatusType == Definitions::MessageStatusType::BUSY);
    REQUIRE(testLink->getPendingCount() == static_cast<size_t>(0));
}

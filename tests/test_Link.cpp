#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
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

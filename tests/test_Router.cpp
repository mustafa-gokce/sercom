#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Clock.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/Router.h"
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
     * Mock transport that queues the sent data until the test receives it and keeps every sent frame.
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
        this->sentFrames.push_back(Frame::Frame(data, size));

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
     * Get every frame sent so far, the dropped ones included.
     */
    [[nodiscard]] std::vector<Frame::Frame> getSentFrames() const
    {
        // expose the sent frames
        return this->sentFrames;
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
    std::vector<Frame::Frame> sentFrames;           // every sent frame
    DataFilter filter;                              // filter that keeps or drops sent data
    size_t sendCount;                               // number of sends so far
};

class MockLinkHandler final : public Link::LinkHandler
{
public:
    /*
     * Mock link handler of a node that records every event and answers with a set message status.
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

class TestRouter
{
public:
    /*
     * Router test fixture with a chain of the host, an upper node 2, a lower node 3 and an end node 4.
     */
    TestRouter()
        : messageCounter(0)
    {
        // initialize clock
        static constexpr std::chrono::milliseconds startTime = 1000ms;
        this->clock.advanceTime(startTime);

        // initialize routers of the chain
        this->createRouters(UpperNodeAddress,
                            LowerNodeAddress,
                            Definitions::UNASSIGNED_ADDRESS,
                            EndNodeAddress);
    }
    ~TestRouter()
    {
        // close routers
        this->closeRouters();
    }

    /*
     * Get the link configuration of every port.
     */
    [[nodiscard]] static Link::LinkConfiguration getLinkConfiguration()
    {
        // shorten the retransmit timeout of the default link configuration
        static constexpr std::chrono::milliseconds retransmitTimeout = 100ms;
        Link::LinkConfiguration linkConfiguration;
        linkConfiguration.retransmitTimeout = retransmitTimeout;

        // expose the link configuration
        return linkConfiguration;
    }

    /*
     * Get the configuration of a router with a node address and a proxy address.
     */
    [[nodiscard]] static Router::RouterConfiguration getRouterConfiguration(
        const uint8_t nodeAddress, const uint8_t proxyAddress)
    {
        // create router configuration
        Router::RouterConfiguration routerConfiguration;
        routerConfiguration.nodeAddress = nodeAddress;
        routerConfiguration.proxyAddress = proxyAddress;

        // expose the router configuration
        return routerConfiguration;
    }

    /*
     * Get the bytes of a routed frame built from its fields.
     */
    [[nodiscard]] static std::vector<uint8_t> getRoutedFrameData(const uint8_t flags,
                                                                 const uint16_t frameMessageCounter,
                                                                 const uint8_t sourceAddress,
                                                                 const uint8_t destinationAddress,
                                                                 const uint8_t timeToLive)
    {
        // build the routed frame with a short payload
        Frame::RouteHeader routeHeader;
        routeHeader.sourceAddress = sourceAddress;
        routeHeader.destinationAddress = destinationAddress;
        routeHeader.timeToLive = timeToLive;
        const std::vector<uint8_t> payload = {0x01, 0x02};
        const Frame::Frame frame(flags,
                                 frameMessageCounter,
                                 TestMessageType,
                                 routeHeader,
                                 payload.data(),
                                 payload.size());

        // expose the bytes of the routed frame
        return std::vector<uint8_t>(frame.getData(), frame.getData() + frame.getSize());
    }

    /*
     * Get the message status that a NACK frame carries.
     */
    [[nodiscard]] static Definitions::MessageStatusType getRejectionStatus(const Frame::Frame &frame)
    {
        // NACK frame must carry its message status
        if (!frame.getFlag(Definitions::FrameFlagType::NACK) || frame.getPayloadLength() == 0) {
            return Definitions::MessageStatusType::UNKNOWN;
        }

        // expose the message status
        return Definitions::toMessageStatusType(frame.getPayload()[0]);
    }

    /*
     * Create the routers of the chain with their addresses and the proxy address of the lower node.
     */
    void createRouters(const uint8_t upperNodeAddress,
                       const uint8_t lowerNodeAddress,
                       const uint8_t lowerProxyAddress,
                       const uint8_t endNodeAddress)
    {
        // close the routers of an earlier chain
        this->closeRouters();

        // initialize routers from the host down to the end node
        const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
        this->hostRouter.reset(
            new Router::Router(this->hostTransport,
                               nullptr,
                               this->hostHandler,
                               this->clock,
                               linkConfiguration,
                               getRouterConfiguration(Definitions::HOST_ADDRESS,
                                                      Definitions::UNASSIGNED_ADDRESS)));
        this->upperRouter.reset(
            new Router::Router(this->upperPrimaryTransport,
                               &this->upperSecondaryTransport,
                               this->upperHandler,
                               this->clock,
                               linkConfiguration,
                               getRouterConfiguration(upperNodeAddress,
                                                      Definitions::UNASSIGNED_ADDRESS)));
        this->lowerRouter.reset(
            new Router::Router(this->lowerPrimaryTransport,
                               &this->lowerSecondaryTransport,
                               this->lowerHandler,
                               this->clock,
                               linkConfiguration,
                               getRouterConfiguration(lowerNodeAddress, lowerProxyAddress)));
        this->endRouter.reset(
            new Router::Router(this->endTransport,
                               nullptr,
                               this->endHandler,
                               this->clock,
                               linkConfiguration,
                               getRouterConfiguration(endNodeAddress,
                                                      Definitions::UNASSIGNED_ADDRESS)));
    }

    /*
     * Close the routers of the chain.
     */
    void closeRouters()
    {
        // close routers from the end node up to the host
        this->endRouter.reset(nullptr);
        this->lowerRouter.reset(nullptr);
        this->upperRouter.reset(nullptr);
        this->hostRouter.reset(nullptr);
    }

    /*
     * Hand the data waiting in a transport to a port of a router.
     */
    static void deliverData(MockTransport &transport,
                            Router::Router &router,
                            const Definitions::PortType portType)
    {
        // hand every waiting data to the port
        std::vector<uint8_t> data;
        while (transport.receiveData(data)) {
            router.onReceivedData(portType, data.data(), data.size());
        }
    }

    /*
     * Deliver the sent data on every link of the chain until no data waits.
     */
    void pump()
    {
        // deliver the sent data a bounded number of rounds
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            // data must wait in a transport
            if (this->hostTransport.getEmpty() && this->upperPrimaryTransport.getEmpty()
                && this->upperSecondaryTransport.getEmpty()
                && this->lowerPrimaryTransport.getEmpty()
                && this->lowerSecondaryTransport.getEmpty() && this->endTransport.getEmpty()) {
                return;
            }

            // deliver the data of every link in both directions
            deliverData(this->hostTransport, *this->upperRouter, Definitions::PortType::PRIMARY);
            deliverData(this->upperPrimaryTransport,
                        *this->hostRouter,
                        Definitions::PortType::PRIMARY);
            deliverData(this->upperSecondaryTransport,
                        *this->lowerRouter,
                        Definitions::PortType::PRIMARY);
            deliverData(this->lowerPrimaryTransport,
                        *this->upperRouter,
                        Definitions::PortType::SECONDARY);
            deliverData(this->lowerSecondaryTransport,
                        *this->endRouter,
                        Definitions::PortType::PRIMARY);
            deliverData(this->endTransport, *this->lowerRouter, Definitions::PortType::SECONDARY);
        }
    }

    /*
     * Let time pass, maintain every router and deliver what they sent.
     */
    void advance(const std::chrono::milliseconds duration)
    {
        // let time pass
        this->clock.advanceTime(duration);

        // maintain every router
        this->hostRouter->maintain();
        this->upperRouter->maintain();
        this->lowerRouter->maintain();
        this->endRouter->maintain();

        // deliver the sent data
        this->pump();
    }

    /*
     * Send an acknowledged test message of the host to a destination and store its message counter.
     */
    [[nodiscard]] Definitions::SendStatusType sendHostMessage(const uint8_t destinationAddress)
    {
        // expose the send status of the message
        const std::vector<uint8_t> payload = {0x10, 0x20, 0x30};
        return this->hostRouter->sendMessage(destinationAddress,
                                             TestMessageType,
                                             payload.data(),
                                             payload.size(),
                                             true,
                                             this->messageCounter);
    }

    // delete copy and move operators
    TestRouter(const TestRouter &other) = delete;
    TestRouter &operator=(const TestRouter &other) = delete;
    TestRouter(TestRouter &&other) noexcept = delete;
    TestRouter &operator=(TestRouter &&other) noexcept = delete;

protected:
    static constexpr uint8_t UpperNodeAddress = 2;      // address of the upper node
    static constexpr uint8_t LowerNodeAddress = 3;      // address of the lower node
    static constexpr uint8_t EndNodeAddress = 4;        // address of the end node
    static constexpr uint8_t MissingNodeAddress = 9;    // address of no node of the chain
    static constexpr uint16_t TestMessageType = 0x0100; // message type of the test messages

protected:
    MockClock clock;                             // clock of every router
    MockTransport hostTransport;                 // data sent by the host
    MockTransport upperPrimaryTransport;         // data sent up by the upper node
    MockTransport upperSecondaryTransport;       // data sent down by the upper node
    MockTransport lowerPrimaryTransport;         // data sent up by the lower node
    MockTransport lowerSecondaryTransport;       // data sent down by the lower node
    MockTransport endTransport;                  // data sent up by the end node
    MockLinkHandler hostHandler;                 // application of the host
    MockLinkHandler upperHandler;                // application of the upper node
    MockLinkHandler lowerHandler;                // application of the lower node
    MockLinkHandler endHandler;                  // application of the end node
    std::unique_ptr<Router::Router> hostRouter;  // router of the host
    std::unique_ptr<Router::Router> upperRouter; // router of the upper node
    std::unique_ptr<Router::Router> lowerRouter; // router of the lower node
    std::unique_ptr<Router::Router> endRouter;   // router of the end node
    uint16_t messageCounter;                     // message counter of the latest sent message
};

TEST_CASE_METHOD(TestRouter,
                 "An own message without a destination is a direct frame for the neighbour",
                 "[Router][testOwnDirectMessage]")
{
    // send a message of the host to its neighbour
    REQUIRE(sendHostMessage(Definitions::UNASSIGNED_ADDRESS) == Definitions::SendStatusType::SENT);
    pump();

    // check the direct frame with the route capable flag of a node with an address
    const std::vector<Frame::Frame> sentFrames = hostTransport.getSentFrames();
    REQUIRE(sentFrames.size() == static_cast<size_t>(1));
    REQUIRE_FALSE(sentFrames.at(0).getRouted());
    REQUIRE(sentFrames.at(0).getFlag(Definitions::FrameFlagType::ROUTE_CAPABLE));

    // check the delivery to the upper node and the confirmation of the host
    REQUIRE(upperHandler.getMessages().size() == static_cast<size_t>(1));
    REQUIRE(upperRouter->getStatistics().localFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(hostHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(hostHandler.getDeliveryResults().at(0).confirmed);
}

TEST_CASE_METHOD(TestRouter,
                 "An own message to a destination is a routed frame with the address of the node",
                 "[Router][testOwnRoutedMessage]")
{
    // send a message of the host to the upper node
    REQUIRE(sendHostMessage(UpperNodeAddress) == Definitions::SendStatusType::SENT);
    pump();

    // check the routed frame with the address and the time to live of the host
    const std::vector<Frame::Frame> sentFrames = hostTransport.getSentFrames();
    REQUIRE(sentFrames.size() == static_cast<size_t>(1));
    REQUIRE(sentFrames.at(0).getRouted());
    REQUIRE(sentFrames.at(0).getSourceAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(sentFrames.at(0).getDestinationAddress() == UpperNodeAddress);
    REQUIRE(sentFrames.at(0).getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);

    // check the delivery to the upper node and the confirmation of the host
    const std::vector<Frame::Frame> messages = upperHandler.getMessages();
    REQUIRE(messages.size() == static_cast<size_t>(1));
    REQUIRE(messages.at(0).getSourceAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(hostHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(hostHandler.getDeliveryResults().at(0).confirmed);
    REQUIRE(hostRouter->getPeerVersionType(Definitions::PortType::PRIMARY)
            == Definitions::PeerVersionType::VERSION_2);
}

TEST_CASE_METHOD(TestRouter,
                 "Own messages need an address, another destination and a peer of version 2",
                 "[Router][testOwnMessageRefused]")
{
    // check an own message to the own address
    REQUIRE(sendHostMessage(Definitions::HOST_ADDRESS)
            == Definitions::SendStatusType::INVALID_ARGUMENT);

    // check the own messages of a lower node without an address
    createRouters(UpperNodeAddress,
                  Definitions::UNASSIGNED_ADDRESS,
                  Definitions::UNASSIGNED_ADDRESS,
                  EndNodeAddress);
    REQUIRE(
        lowerRouter->sendMessage(EndNodeAddress, TestMessageType, nullptr, 0, false, messageCounter)
        == Definitions::SendStatusType::NO_ROUTE);
    REQUIRE(lowerRouter->sendMessage(Definitions::UNASSIGNED_ADDRESS,
                                     TestMessageType,
                                     nullptr,
                                     0,
                                     false,
                                     messageCounter)
            == Definitions::SendStatusType::SENT);

    // let the end node learn that its peer reads version 1 only
    REQUIRE(endRouter->sendMessage(Definitions::UNASSIGNED_ADDRESS,
                                   TestMessageType,
                                   nullptr,
                                   0,
                                   true,
                                   messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(endRouter->getPeerVersionType(Definitions::PortType::PRIMARY)
            == Definitions::PeerVersionType::VERSION_1);

    // check the direct frame for the host and the refused routed frame
    REQUIRE(endRouter->sendMessage(Definitions::HOST_ADDRESS,
                                   TestMessageType,
                                   nullptr,
                                   0,
                                   false,
                                   messageCounter)
            == Definitions::SendStatusType::SENT);
    REQUIRE_FALSE(endTransport.getSentFrames().back().getRouted());
    REQUIRE(
        endRouter->sendMessage(UpperNodeAddress, TestMessageType, nullptr, 0, false, messageCounter)
        == Definitions::SendStatusType::NO_ROUTE);
}

TEST_CASE_METHOD(TestRouter,
                 "A direct frame of the secondary port is dropped as unreachable",
                 "[Router][testDirectFrameFromBelow]")
{
    // send an acknowledged direct message of the lower node up to the upper node
    REQUIRE(lowerRouter->sendMessage(Definitions::UNASSIGNED_ADDRESS,
                                     TestMessageType,
                                     nullptr,
                                     0,
                                     true,
                                     messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the rejection without a delivery
    const std::vector<DeliveryResult> deliveryResults = lowerHandler.getDeliveryResults();
    REQUIRE(upperHandler.getMessages().empty());
    REQUIRE(upperRouter->getStatistics().unreachableFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(deliveryResults.size() == static_cast<size_t>(1));
    REQUIRE(deliveryResults.at(0).deliveryFailureType == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(deliveryResults.at(0).remoteMessageStatusType
            == Definitions::MessageStatusType::UNREACHABLE);
}

TEST_CASE_METHOD(TestRouter,
                 "A node without an address drops every routed frame and answers in version 1",
                 "[Router][testUnaddressedNode]")
{
    // send a routed message of the host to an upper node without an address
    createRouters(Definitions::UNASSIGNED_ADDRESS,
                  LowerNodeAddress,
                  Definitions::UNASSIGNED_ADDRESS,
                  EndNodeAddress);
    REQUIRE(sendHostMessage(UpperNodeAddress) == Definitions::SendStatusType::SENT);
    pump();

    // check the rejection in a frame of version 1 without the route capable flag
    const std::vector<Frame::Frame> sentFrames = upperPrimaryTransport.getSentFrames();
    REQUIRE(sentFrames.size() == static_cast<size_t>(1));
    REQUIRE(getRejectionStatus(sentFrames.at(0)) == Definitions::MessageStatusType::UNREACHABLE);
    REQUIRE_FALSE(sentFrames.at(0).getFlag(Definitions::FrameFlagType::ROUTE_CAPABLE));
    REQUIRE(upperHandler.getMessages().empty());
    REQUIRE(hostHandler.getDeliveryResults().at(0).remoteMessageStatusType
            == Definitions::MessageStatusType::UNREACHABLE);

    // check that the host sends no routed frame to its peer of version 1 any more
    REQUIRE(hostRouter->getPeerVersionType(Definitions::PortType::PRIMARY)
            == Definitions::PeerVersionType::VERSION_1);
    REQUIRE(sendHostMessage(LowerNodeAddress) == Definitions::SendStatusType::NO_ROUTE);
}

TEST_CASE_METHOD(TestRouter,
                 "A routed frame without a source or a destination of a node is dropped",
                 "[Router][testRoutedFrameWithoutAddresses]")
{
    // hand routed frames with unassigned and broadcast addresses to the upper node
    static const std::vector<std::vector<uint8_t>> frames = {
        getRoutedFrameData(0x11,
                           1,
                           Definitions::UNASSIGNED_ADDRESS,
                           UpperNodeAddress,
                           Settings::DEFAULT_TIME_TO_LIVE),
        getRoutedFrameData(0x11,
                           2,
                           Definitions::BROADCAST_ADDRESS,
                           UpperNodeAddress,
                           Settings::DEFAULT_TIME_TO_LIVE),
        getRoutedFrameData(0x11,
                           3,
                           Definitions::HOST_ADDRESS,
                           Definitions::UNASSIGNED_ADDRESS,
                           Settings::DEFAULT_TIME_TO_LIVE),
    };
    std::for_each(frames.cbegin(), frames.cend(), [this](const std::vector<uint8_t> &frame) {
        upperRouter->onReceivedData(Definitions::PortType::PRIMARY, frame.data(), frame.size());
    });

    // check the rejections without a delivery
    const std::vector<Frame::Frame> sentFrames = upperPrimaryTransport.getSentFrames();
    REQUIRE(sentFrames.size() == frames.size());
    std::for_each(sentFrames.cbegin(), sentFrames.cend(), [](const Frame::Frame &sentFrame) {
        // check the rejection
        CAPTURE(sentFrame.getMessageCounter());
        REQUIRE(getRejectionStatus(sentFrame) == Definitions::MessageStatusType::UNREACHABLE);
    });
    REQUIRE(upperHandler.getMessages().empty());
    REQUIRE(upperRouter->getStatistics().unreachableFrameCounter == frames.size());
}

TEST_CASE_METHOD(TestRouter,
                 "A routed frame of the own source is dropped as a loop",
                 "[Router][testLoop]")
{
    // hand a routed frame that the upper node sent itself back to the upper node
    const std::vector<uint8_t> frame = getRoutedFrameData(0x11,
                                                          1,
                                                          UpperNodeAddress,
                                                          LowerNodeAddress,
                                                          Settings::DEFAULT_TIME_TO_LIVE);
    upperRouter->onReceivedData(Definitions::PortType::PRIMARY, frame.data(), frame.size());

    // check the rejection as a loop
    const std::vector<Frame::Frame> sentFrames = upperPrimaryTransport.getSentFrames();
    REQUIRE(sentFrames.size() == static_cast<size_t>(1));
    REQUIRE(getRejectionStatus(sentFrames.at(0)) == Definitions::MessageStatusType::UNREACHABLE);
    REQUIRE(upperRouter->getStatistics().loopedFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(upperSecondaryTransport.getSentFrames().empty());
}

TEST_CASE_METHOD(TestRouter,
                 "A routed frame for another node is dropped by a node without another port",
                 "[Router][testNoOtherPort]")
{
    // hand a routed frame for a missing node to the end node
    const std::vector<uint8_t> frame = getRoutedFrameData(0x11,
                                                          1,
                                                          Definitions::HOST_ADDRESS,
                                                          MissingNodeAddress,
                                                          Settings::DEFAULT_TIME_TO_LIVE);
    endRouter->onReceivedData(Definitions::PortType::PRIMARY, frame.data(), frame.size());

    // check the rejection without a delivery
    const std::vector<Frame::Frame> sentFrames = endTransport.getSentFrames();
    REQUIRE(sentFrames.size() == static_cast<size_t>(1));
    REQUIRE(getRejectionStatus(sentFrames.at(0)) == Definitions::MessageStatusType::UNREACHABLE);
    REQUIRE(endHandler.getMessages().empty());
    REQUIRE(endRouter->getStatistics().unreachableFrameCounter == static_cast<uint64_t>(1));
}

TEST_CASE_METHOD(
    TestRouter,
    "A request travels down with one link less to live on every hop and so does its answer up",
    "[Router][testForwarding]")
{
    // send a message of the host to the end node
    REQUIRE(sendHostMessage(EndNodeAddress) == Definitions::SendStatusType::SENT);
    pump();

    // check the time to live on every hop down
    REQUIRE(hostTransport.getSentFrames().at(0).getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(upperSecondaryTransport.getSentFrames().at(0).getTimeToLive()
            == Settings::DEFAULT_TIME_TO_LIVE - 1);
    REQUIRE(lowerSecondaryTransport.getSentFrames().at(0).getTimeToLive()
            == Settings::DEFAULT_TIME_TO_LIVE - 2);

    // check the delivery to the end node only and the confirmation of the first hop
    const std::vector<Frame::Frame> endMessages = endHandler.getMessages();
    REQUIRE(endMessages.size() == static_cast<size_t>(1));
    REQUIRE(endMessages.at(0).getSourceAddress() == Definitions::HOST_ADDRESS);
    REQUIRE(upperHandler.getMessages().empty());
    REQUIRE(lowerHandler.getMessages().empty());
    REQUIRE(hostHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(hostHandler.getDeliveryResults().at(0).confirmed);

    // answer the host from the end node
    REQUIRE(endRouter->sendMessage(Definitions::HOST_ADDRESS,
                                   TestMessageType,
                                   nullptr,
                                   0,
                                   true,
                                   messageCounter)
            == Definitions::SendStatusType::SENT);
    pump();

    // check the time to live on every hop up and the delivery to the host
    REQUIRE(endTransport.getSentFrames().back().getTimeToLive() == Settings::DEFAULT_TIME_TO_LIVE);
    REQUIRE(lowerPrimaryTransport.getSentFrames().back().getTimeToLive()
            == Settings::DEFAULT_TIME_TO_LIVE - 1);
    REQUIRE(upperPrimaryTransport.getSentFrames().back().getTimeToLive()
            == Settings::DEFAULT_TIME_TO_LIVE - 2);
    const std::vector<Frame::Frame> hostMessages = hostHandler.getMessages();
    REQUIRE(hostMessages.size() == static_cast<size_t>(1));
    REQUIRE(hostMessages.at(0).getSourceAddress() == EndNodeAddress);
    REQUIRE(endHandler.getDeliveryResults().size() == static_cast<size_t>(1));
    REQUIRE(endHandler.getDeliveryResults().at(0).confirmed);

    // check that the outcomes of the forwarded messages stay in the routers
    REQUIRE(upperHandler.getDeliveryResults().empty());
    REQUIRE(lowerHandler.getDeliveryResults().empty());
    REQUIRE(upperRouter->getStatistics().forwardedFrameCounter == static_cast<uint64_t>(2));
    REQUIRE(lowerRouter->getStatistics().forwardedFrameCounter == static_cast<uint64_t>(2));
}

TEST_CASE_METHOD(
    TestRouter,
    "A frame for the host from above and a frame at the end of its time to live are dropped",
    "[Router][testHostRuleAndTimeToLive]")
{
    // hand the upper node a frame for the host from above, one with one link and one with two links to live
    const std::vector<uint8_t> hostFrame = getRoutedFrameData(0x11,
                                                              1,
                                                              LowerNodeAddress,
                                                              Definitions::HOST_ADDRESS,
                                                              Settings::DEFAULT_TIME_TO_LIVE);
    const std::vector<uint8_t> lastLinkFrame = getRoutedFrameData(0x11,
                                                                  2,
                                                                  Definitions::HOST_ADDRESS,
                                                                  EndNodeAddress,
                                                                  1);
    const std::vector<uint8_t> twoLinksFrame = getRoutedFrameData(0x11,
                                                                  3,
                                                                  Definitions::HOST_ADDRESS,
                                                                  EndNodeAddress,
                                                                  2);
    upperRouter->onReceivedData(Definitions::PortType::PRIMARY, hostFrame.data(), hostFrame.size());
    upperRouter->onReceivedData(Definitions::PortType::PRIMARY,
                                lastLinkFrame.data(),
                                lastLinkFrame.size());
    upperRouter->onReceivedData(Definitions::PortType::PRIMARY,
                                twoLinksFrame.data(),
                                twoLinksFrame.size());
    pump();

    // check the answers of the upper node to the first two frames
    const std::vector<Frame::Frame> upperFrames = upperPrimaryTransport.getSentFrames();
    REQUIRE(upperFrames.size() >= static_cast<size_t>(3));
    REQUIRE(getRejectionStatus(upperFrames.at(0)) == Definitions::MessageStatusType::UNREACHABLE);
    REQUIRE(getRejectionStatus(upperFrames.at(1)) == Definitions::MessageStatusType::EXPIRED);
    REQUIRE(upperFrames.at(2).getFlag(Definitions::FrameFlagType::ACK));

    // check the expiry of the third frame at the lower node
    const std::vector<Frame::Frame> lowerFrames = lowerPrimaryTransport.getSentFrames();
    REQUIRE(lowerFrames.size() == static_cast<size_t>(1));
    REQUIRE(getRejectionStatus(lowerFrames.at(0)) == Definitions::MessageStatusType::EXPIRED);
    REQUIRE(upperRouter->getStatistics().expiredFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(upperRouter->getStatistics().failedForwardCounter == static_cast<uint64_t>(1));
    REQUIRE(lowerRouter->getStatistics().expiredFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(endHandler.getMessages().empty());
}

TEST_CASE_METHOD(TestRouter,
                 "A full window of the next link is answered with BUSY until the sender gives up",
                 "[Router][testBusy]")
{
    // let the link from the upper node to the lower node drop everything
    upperSecondaryTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });

    // fill the window of the next link of the upper node
    for (size_t messageIndex = 0; messageIndex < Settings::MaximumPendingFrames; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(sendHostMessage(EndNodeAddress) == Definitions::SendStatusType::SENT);
        pump();
        REQUIRE(hostHandler.getDeliveryResults().back().confirmed);
    }

    // check that one more message is answered with BUSY and kept by the host
    REQUIRE(sendHostMessage(EndNodeAddress) == Definitions::SendStatusType::SENT);
    pump();
    REQUIRE(getRejectionStatus(upperPrimaryTransport.getSentFrames().back())
            == Definitions::MessageStatusType::BUSY);
    REQUIRE(hostHandler.getDeliveryResults().size() == Settings::MaximumPendingFrames);
    REQUIRE(hostRouter->getPendingCount() == static_cast<size_t>(1));

    // let the host retransmit the message into the full window until no retransmission is left
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission < linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        advance(linkConfiguration.retransmitTimeout);
    }

    // check that the last BUSY ends the delivery
    const std::vector<DeliveryResult> deliveryResults = hostHandler.getDeliveryResults();
    REQUIRE(deliveryResults.size() == Settings::MaximumPendingFrames + 1);
    REQUIRE(deliveryResults.back().deliveryFailureType
            == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(deliveryResults.back().remoteMessageStatusType == Definitions::MessageStatusType::BUSY);
    REQUIRE(upperRouter->getStatistics().busyFrameCounter
            == static_cast<uint64_t>(linkConfiguration.maximumRetransmissions) + 1);
}

TEST_CASE_METHOD(TestRouter,
                 "A message from below that a router refuses as busy is retransmitted to the host",
                 "[Router][testBusyMessageFromBelow]")
{
    // let the upper node answer no frame of the lower node
    upperSecondaryTransport.setFilter(
        []([[maybe_unused]] const size_t sendCount,
           [[maybe_unused]] const std::vector<uint8_t> &data) noexcept { return false; });

    // fill the window of the lower node towards the host with messages of the end node
    const std::vector<uint8_t> payload = {0x40, 0x50};
    for (size_t messageIndex = 0; messageIndex <= Settings::MaximumPendingFrames; ++messageIndex) {
        CAPTURE(messageIndex);
        REQUIRE(endRouter->sendMessage(Definitions::HOST_ADDRESS,
                                       TestMessageType,
                                       payload.data(),
                                       payload.size(),
                                       true,
                                       messageCounter)
                == Definitions::SendStatusType::SENT);
        pump();
    }

    // check that the lower node refused the last message as busy and the end node kept it
    REQUIRE(getRejectionStatus(lowerSecondaryTransport.getSentFrames().back())
            == Definitions::MessageStatusType::BUSY);
    REQUIRE(endHandler.getDeliveryResults().size() == Settings::MaximumPendingFrames);
    REQUIRE(endRouter->getPendingCount() == static_cast<size_t>(1));
    REQUIRE(hostHandler.getMessages().size() == Settings::MaximumPendingFrames);

    // let the upper node answer again and the retransmissions pass
    upperSecondaryTransport.setFilter(DataFilter());
    const Link::LinkConfiguration linkConfiguration = getLinkConfiguration();
    for (uint8_t retransmission = 0; retransmission < linkConfiguration.maximumRetransmissions;
         ++retransmission) {
        CAPTURE(retransmission);
        advance(linkConfiguration.retransmitTimeout);
    }

    // check that every message reached the host once and the end node lost none
    const std::vector<DeliveryResult> deliveryResults = endHandler.getDeliveryResults();
    REQUIRE(hostHandler.getMessages().size() == Settings::MaximumPendingFrames + 1);
    REQUIRE(deliveryResults.size() == Settings::MaximumPendingFrames + 1);
    REQUIRE(
        std::all_of(deliveryResults.cbegin(),
                    deliveryResults.cend(),
                    [](const DeliveryResult &deliveryResult) { return deliveryResult.confirmed; }));
    REQUIRE(lowerRouter->getStatistics().busyFrameCounter >= static_cast<uint64_t>(1));
    REQUIRE(endRouter->getLinkStatistics(Definitions::PortType::PRIMARY).failedDeliveryCounter
            == static_cast<uint64_t>(0));
}

TEST_CASE_METHOD(TestRouter,
                 "A ring wiring is stopped by the own source rule and by the time to live",
                 "[Router][testRing]")
{
    // wire two nodes in a ring, each secondary port to the primary port of the other node
    MockTransport firstPrimaryTransport;
    MockTransport firstSecondaryTransport;
    MockTransport secondPrimaryTransport;
    MockTransport secondSecondaryTransport;
    MockLinkHandler firstHandler;
    MockLinkHandler secondHandler;
    Router::Router firstRouter(firstPrimaryTransport,
                               &firstSecondaryTransport,
                               firstHandler,
                               clock,
                               getLinkConfiguration(),
                               getRouterConfiguration(UpperNodeAddress,
                                                      Definitions::UNASSIGNED_ADDRESS));
    Router::Router secondRouter(secondPrimaryTransport,
                                &secondSecondaryTransport,
                                secondHandler,
                                clock,
                                getLinkConfiguration(),
                                getRouterConfiguration(LowerNodeAddress,
                                                       Definitions::UNASSIGNED_ADDRESS));
    const auto pumpRing = [&firstPrimaryTransport,
                           &firstSecondaryTransport,
                           &secondPrimaryTransport,
                           &secondSecondaryTransport,
                           &firstRouter,
                           &secondRouter]() {
        // deliver the data of the ring until none waits
        static constexpr int maximumAttemptCount = 1000;
        for (int attempt = 0; attempt < maximumAttemptCount; ++attempt) {
            deliverData(firstSecondaryTransport, secondRouter, Definitions::PortType::PRIMARY);
            deliverData(secondPrimaryTransport, firstRouter, Definitions::PortType::SECONDARY);
            deliverData(secondSecondaryTransport, firstRouter, Definitions::PortType::PRIMARY);
            deliverData(firstPrimaryTransport, secondRouter, Definitions::PortType::SECONDARY);
        }
    };

    // send an own message of the first node to a missing node around the ring
    REQUIRE(firstRouter
                .sendMessage(MissingNodeAddress, TestMessageType, nullptr, 0, false, messageCounter)
            == Definitions::SendStatusType::SENT);
    pumpRing();

    // check that the first node dropped its own frame as a loop
    REQUIRE(secondRouter.getStatistics().forwardedFrameCounter == static_cast<uint64_t>(1));
    REQUIRE(firstRouter.getStatistics().loopedFrameCounter == static_cast<uint64_t>(1));

    // hand the first node a frame of another source for a missing node
    const std::vector<uint8_t> frame = getRoutedFrameData(0x10,
                                                          1,
                                                          EndNodeAddress,
                                                          MissingNodeAddress,
                                                          Settings::DEFAULT_TIME_TO_LIVE);
    firstRouter.onReceivedData(Definitions::PortType::PRIMARY, frame.data(), frame.size());
    pumpRing();

    // check that the frame circled until its time to live ran out
    const uint64_t forwardedFrameCount = firstRouter.getStatistics().forwardedFrameCounter
                                         + secondRouter.getStatistics().forwardedFrameCounter;
    const uint64_t expiredFrameCount = firstRouter.getStatistics().expiredFrameCounter
                                       + secondRouter.getStatistics().expiredFrameCounter;
    REQUIRE(forwardedFrameCount == static_cast<uint64_t>(1 + Settings::DEFAULT_TIME_TO_LIVE - 1));
    REQUIRE(expiredFrameCount == static_cast<uint64_t>(1));
    REQUIRE(firstHandler.getMessages().empty());
    REQUIRE(secondHandler.getMessages().empty());
}

TEST_CASE_METHOD(TestRouter,
                 "A lost frame on one hop is sent again by that hop and every request arrives once",
                 "[Router][testLossOnOneHop]")
{
    // lose a tenth of the frames between the upper and the lower node with a fixed seed
    static constexpr uint32_t seed = 42;
    static constexpr uint32_t lossPercentage = 10;
    std::mt19937 generator(seed);
    const DataFilter lossFilter = [&generator]([[maybe_unused]] const size_t sendCount,
                                               [[maybe_unused]] const std::vector<uint8_t> &data) {
        static constexpr uint32_t percentageCount = 100;
        return generator() % percentageCount >= lossPercentage;
    };
    upperSecondaryTransport.setFilter(lossFilter);
    lowerPrimaryTransport.setFilter(lossFilter);

    // send numbered messages of the host to the end node whenever the host may send
    static constexpr uint8_t messageCount = 50;
    uint8_t sentMessageCount = 0;
    static constexpr int maximumRoundCount = 10000;
    for (int round = 0; round < maximumRoundCount && endHandler.getMessages().size() < messageCount;
         ++round) {
        // send the next message when the window allows it
        if (sentMessageCount < messageCount
            && hostRouter->getSendAcknowledgedAllowed(EndNodeAddress)) {
            const std::vector<uint8_t> payload = {sentMessageCount};
            REQUIRE(hostRouter->sendMessage(EndNodeAddress,
                                            TestMessageType,
                                            payload.data(),
                                            payload.size(),
                                            true,
                                            messageCounter)
                    == Definitions::SendStatusType::SENT);
            ++sentMessageCount;
            pump();
        }

        // let a retransmit timeout pass
        advance(getLinkConfiguration().retransmitTimeout);
    }

    // check that every message arrived once and in no other copy
    const std::vector<Frame::Frame> endMessages = endHandler.getMessages();
    REQUIRE(endMessages.size() == static_cast<size_t>(messageCount));
    for (uint8_t messageIndex = 0; messageIndex < messageCount; ++messageIndex) {
        CAPTURE(messageIndex);
        const size_t copyCount = static_cast<size_t>(
            std::count_if(endMessages.cbegin(),
                          endMessages.cend(),
                          [messageIndex](const Frame::Frame &endMessage) {
                              return endMessage.getPayload()[0] == messageIndex;
                          }));
        REQUIRE(copyCount == static_cast<size_t>(1));
    }

    // check the retransmissions on the lossy hop only
    REQUIRE(upperRouter->getLinkStatistics(Definitions::PortType::SECONDARY).retransmissionCounter
            > static_cast<uint64_t>(0));
    REQUIRE(hostRouter->getLinkStatistics(Definitions::PortType::PRIMARY).retransmissionCounter
            == static_cast<uint64_t>(0));
    REQUIRE(upperRouter->getStatistics().failedForwardCounter == static_cast<uint64_t>(0));
}

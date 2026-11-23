#include <cstdint>
#include <memory>
#include <vector>

#include <catch2/catch_all.hpp>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/Link.h"
#include "../core/include/RoutePort.h"

using namespace sercom;

namespace {

/*
 * Link event that a mock route port handler saw.
 */
struct PortEvent
{
    Definitions::PortType portType = Definitions::PortType::UNKNOWN; // port of the event
    uint16_t messageCounter = 0; // message counter of the message
    uint16_t messageType = 0;    // message type of the message
    bool received = false;       // event is a received message
    bool confirmed = false;      // message reached the peer
    Definitions::DeliveryFailureType deliveryFailureType
        = Definitions::DeliveryFailureType::UNKNOWN; // reason of a failed delivery
    Definitions::MessageStatusType remoteMessageStatusType
        = Definitions::MessageStatusType::UNKNOWN; // message status of a rejection
};

class MockRoutePortHandler final : public RoutePort::RoutePortHandler
{
public:
    /*
     * Mock route port handler that records every event and answers with a set message status.
     */
    MockRoutePortHandler()
        : messageStatusType(Definitions::MessageStatusType::ACCEPTED)
    {}
    ~MockRoutePortHandler() final {}

    // delete copy constructor and copy assignment operator
    MockRoutePortHandler(const MockRoutePortHandler &) = delete;
    MockRoutePortHandler &operator=(const MockRoutePortHandler &) = delete;

    // delete move constructor and move assignment operator
    MockRoutePortHandler(MockRoutePortHandler &&) = delete;
    MockRoutePortHandler &operator=(MockRoutePortHandler &&) = delete;

public:
    /*
     * Listen for a received message of a port, record it and get the set message status.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedPortMessage(
        const Definitions::PortType portType, const Frame::Frame &frame) final
    {
        // record the received message
        PortEvent portEvent;
        portEvent.portType = portType;
        portEvent.messageCounter = frame.getMessageCounter();
        portEvent.messageType = frame.getMessageType();
        portEvent.received = true;
        this->portEvents.push_back(portEvent);

        // expose the set message status
        return this->messageStatusType;
    }

    /*
     * Listen for a confirmed delivery of a port and record it.
     */
    void onPortDeliveryConfirmed(const Definitions::PortType portType,
                                 const uint16_t messageCounter,
                                 const uint16_t messageType) final
    {
        // record the confirmed delivery
        PortEvent portEvent;
        portEvent.portType = portType;
        portEvent.messageCounter = messageCounter;
        portEvent.messageType = messageType;
        portEvent.confirmed = true;
        this->portEvents.push_back(portEvent);
    }

    /*
     * Listen for a failed delivery of a port and record it.
     */
    void onPortDeliveryFailed(const Definitions::PortType portType,
                              const uint16_t messageCounter,
                              const uint16_t messageType,
                              const Definitions::DeliveryFailureType deliveryFailureType,
                              const Definitions::MessageStatusType remoteMessageStatusType) final
    {
        // record the failed delivery
        PortEvent portEvent;
        portEvent.portType = portType;
        portEvent.messageCounter = messageCounter;
        portEvent.messageType = messageType;
        portEvent.deliveryFailureType = deliveryFailureType;
        portEvent.remoteMessageStatusType = remoteMessageStatusType;
        this->portEvents.push_back(portEvent);
    }

public:
    /*
     * Get the recorded events in arrival order.
     */
    [[nodiscard]] std::vector<PortEvent> getPortEvents() const
    {
        // expose the recorded events
        return this->portEvents;
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
    std::vector<PortEvent> portEvents;                // recorded events
    Definitions::MessageStatusType messageStatusType; // answer to a received message
};

} // namespace

class TestRoutePort
{
public:
    /*
     * Route port test fixture.
     */
    TestRoutePort()
    {
        // initialize route ports
        this->primaryPort.reset(
            new RoutePort::RoutePort(this->handler, Definitions::PortType::PRIMARY));
        this->secondaryPort.reset(
            new RoutePort::RoutePort(this->handler, Definitions::PortType::SECONDARY));
    }
    ~TestRoutePort()
    {
        // close route ports
        this->secondaryPort.reset(nullptr);
        this->primaryPort.reset(nullptr);
    }

    // delete copy and move operators
    TestRoutePort(const TestRoutePort &other) = delete;
    TestRoutePort &operator=(const TestRoutePort &other) = delete;
    TestRoutePort(TestRoutePort &&other) noexcept = delete;
    TestRoutePort &operator=(TestRoutePort &&other) noexcept = delete;

protected:
    MockRoutePortHandler handler;                        // events of both ports
    std::unique_ptr<RoutePort::RoutePort> primaryPort;   // port towards the host
    std::unique_ptr<RoutePort::RoutePort> secondaryPort; // port away from the host
};

TEST_CASE_METHOD(TestRoutePort,
                 "Each port hands a received message to the handler with its port type",
                 "[RoutePort][testReceivedMessage]")
{
    // check the port types
    REQUIRE(primaryPort->getPortType() == Definitions::PortType::PRIMARY);
    REQUIRE(secondaryPort->getPortType() == Definitions::PortType::SECONDARY);

    // hand a message to each port as its link does
    const Frame::Frame primaryFrame(0x01, 7, 0x0001, nullptr, 0);
    const Frame::Frame secondaryFrame(0x01, 8, 0x0003, nullptr, 0);
    Link::LinkHandler &primaryLinkHandler = *primaryPort;
    Link::LinkHandler &secondaryLinkHandler = *secondaryPort;
    REQUIRE(primaryLinkHandler.onReceivedMessage(primaryFrame)
            == Definitions::MessageStatusType::ACCEPTED);
    handler.setMessageStatusType(Definitions::MessageStatusType::UNREACHABLE);
    REQUIRE(secondaryLinkHandler.onReceivedMessage(secondaryFrame)
            == Definitions::MessageStatusType::UNREACHABLE);

    // check both messages with their port types
    const std::vector<PortEvent> portEvents = handler.getPortEvents();
    REQUIRE(portEvents.size() == static_cast<size_t>(2));
    REQUIRE(portEvents.at(0).received);
    REQUIRE(portEvents.at(0).portType == Definitions::PortType::PRIMARY);
    REQUIRE(portEvents.at(0).messageCounter == 7);
    REQUIRE(portEvents.at(1).received);
    REQUIRE(portEvents.at(1).portType == Definitions::PortType::SECONDARY);
    REQUIRE(portEvents.at(1).messageType == 0x0003);
}

TEST_CASE_METHOD(
    TestRoutePort,
    "Each port hands confirmed and failed deliveries to the handler with its port type",
    "[RoutePort][testDeliveries]")
{
    // hand a confirmed delivery to the primary port and a failed one to the secondary port
    Link::LinkHandler &primaryLinkHandler = *primaryPort;
    Link::LinkHandler &secondaryLinkHandler = *secondaryPort;
    primaryLinkHandler.onDeliveryConfirmed(3, 0x0100);
    secondaryLinkHandler.onDeliveryFailed(4,
                                          0x0102,
                                          Definitions::DeliveryFailureType::REJECTED,
                                          Definitions::MessageStatusType::EXPIRED);

    // check both deliveries with their port types
    const std::vector<PortEvent> portEvents = handler.getPortEvents();
    REQUIRE(portEvents.size() == static_cast<size_t>(2));
    REQUIRE(portEvents.at(0).portType == Definitions::PortType::PRIMARY);
    REQUIRE(portEvents.at(0).confirmed);
    REQUIRE(portEvents.at(0).messageCounter == 3);
    REQUIRE(portEvents.at(0).messageType == 0x0100);
    REQUIRE(portEvents.at(1).portType == Definitions::PortType::SECONDARY);
    REQUIRE_FALSE(portEvents.at(1).confirmed);
    REQUIRE(portEvents.at(1).messageCounter == 4);
    REQUIRE(portEvents.at(1).deliveryFailureType == Definitions::DeliveryFailureType::REJECTED);
    REQUIRE(portEvents.at(1).remoteMessageStatusType == Definitions::MessageStatusType::EXPIRED);
}

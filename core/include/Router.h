#ifndef SERCOM_ROUTER_H
#define SERCOM_ROUTER_H

#include <cstddef>
#include <cstdint>

#include "../include/Clock.h"
#include "../include/Definitions.h"
#include "../include/Frame.h"
#include "../include/Link.h"
#include "../include/NullTransport.h"
#include "../include/RoutePort.h"
#include "../include/Settings.h"
#include "../include/Transport.h"

namespace sercom::Router {

/*
 * Router configuration.
 */
struct RouterConfiguration
{
    uint8_t nodeAddress = Definitions::UNASSIGNED_ADDRESS;  // address of the node or unassigned
    uint8_t proxyAddress = Definitions::UNASSIGNED_ADDRESS; // address of a neighbour without one
    uint8_t timeToLive = Settings::DEFAULT_TIME_TO_LIVE;    // links an own routed frame may cross
};

/*
 * Router statistics since construction.
 */
struct RouterStatistics
{
    uint64_t localFrameCounter = 0;       // frames delivered to the node
    uint64_t unreachableFrameCounter = 0; // frames dropped without a way to their destination
    uint64_t loopedFrameCounter = 0;      // frames dropped that came back to their source
};

class Router final : public RoutePort::RoutePortHandler
{
public:
    /*
     * Network layer of a node with a primary port towards the host and a secondary port away from it.
     * A node without a secondary transport has a secondary port on a null transport.
     * Local frames and the outcomes of the own messages go to the handler, other frames stay here.
     */
    explicit Router(Transport::Transport &primaryTransport,
                    Transport::Transport *secondaryTransport,
                    Link::LinkHandler &handler,
                    const Clock::Clock &clock,
                    const Link::LinkConfiguration &linkConfiguration,
                    const RouterConfiguration &routerConfiguration);
    ~Router() final;

    // delete copy constructor and copy assignment operator
    Router(const Router &) = delete;
    Router &operator=(const Router &) = delete;

    // delete move constructor and move assignment operator
    Router(Router &&) = delete;
    Router &operator=(Router &&) = delete;

public:
    /*
     * Send an own message to the destination address and store the message counter it carries.
     * An unassigned destination address sends a direct frame to the neighbour on the primary port.
     */
    [[nodiscard]] Definitions::SendStatusType sendMessage(uint8_t destinationAddress,
                                                          uint16_t messageType,
                                                          const uint8_t *payload,
                                                          size_t payloadLength,
                                                          bool acknowledged,
                                                          uint16_t &messageCounter);

    /*
     * Listen for received data of a port and hand it to the link of the port.
     */
    void onReceivedData(Definitions::PortType portType, const uint8_t *data, size_t size);

    /*
     * Maintain the links of both ports.
     */
    void maintain();

public:
    /*
     * Get whether an acknowledged own message to the destination address can be sent now.
     * Every own message leaves on the primary port.
     */
    [[nodiscard]] bool getSendAcknowledgedAllowed(uint8_t destinationAddress) const;

    /*
     * Get the number of acknowledged messages of the primary port that wait for an answer.
     */
    [[nodiscard]] size_t getPendingCount() const;

    /*
     * Get the address of the node, unassigned for a node without an address.
     */
    [[nodiscard]] uint8_t getNodeAddress() const;

    /*
     * Get the link statistics of a port.
     */
    [[nodiscard]] Link::LinkStatistics getLinkStatistics(Definitions::PortType portType) const;

    /*
     * Get the protocol version of the peer of a port.
     */
    [[nodiscard]] Definitions::PeerVersionType getPeerVersionType(
        Definitions::PortType portType) const;

    /*
     * Get the router statistics.
     */
    [[nodiscard]] RouterStatistics getStatistics() const;

private:
    /*
     * Listen for a received message of a port and get the message status that answers it.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedPortMessage(
        Definitions::PortType portType, const Frame::Frame &frame) final;

    /*
     * Listen for an acknowledged message of a port that reached the peer.
     */
    void onPortDeliveryConfirmed(Definitions::PortType portType,
                                 uint16_t messageCounter,
                                 uint16_t messageType) final;

    /*
     * Listen for an acknowledged message of a port that timed out or was rejected by the peer.
     */
    void onPortDeliveryFailed(Definitions::PortType portType,
                              uint16_t messageCounter,
                              uint16_t messageType,
                              Definitions::DeliveryFailureType deliveryFailureType,
                              Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    /*
     * Handle a direct frame that arrived on a port and get the answer to the previous hop.
     */
    [[nodiscard]] Definitions::MessageStatusType handleDirectMessage(Definitions::PortType portType,
                                                                     const Frame::Frame &frame);

    /*
     * Handle a routed frame that arrived on a port and get the answer to the previous hop.
     */
    [[nodiscard]] Definitions::MessageStatusType handleRoutedMessage(Definitions::PortType portType,
                                                                     const Frame::Frame &frame);

    /*
     * Deliver a frame to the node and get the message status of the handler.
     */
    [[nodiscard]] Definitions::MessageStatusType deliverMessage(const Frame::Frame &frame);

    /*
     * Drop a frame that has no way to its destination and get the unreachable status.
     */
    [[nodiscard]] Definitions::MessageStatusType dropUnreachableMessage();

    /*
     * Get the link of a port, the link of the primary port for every port but the secondary one.
     */
    [[nodiscard]] Link::Link &getLink(Definitions::PortType portType);
    [[nodiscard]] const Link::Link &getLink(Definitions::PortType portType) const;

private:
    const RouterConfiguration configuration;    // router configuration
    Link::LinkHandler &handler;                 // reference to handler
    NullTransport::NullTransport nullTransport; // transport of a port without a cable
    RoutePort::RoutePort primaryPort;           // port towards the host
    RoutePort::RoutePort secondaryPort;         // port away from the host
    Link::Link primaryLink;                     // link of the primary port
    Link::Link secondaryLink;                   // link of the secondary port
    RouterStatistics statistics;                // router statistics
};

} // namespace sercom::Router

#endif // SERCOM_ROUTER_H

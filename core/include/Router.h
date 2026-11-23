#ifndef SERCOM_ROUTER_H
#define SERCOM_ROUTER_H

#include <array>
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
    uint64_t forwardedFrameCounter = 0;   // frames passed on to the other port
    uint64_t broadcastFrameCounter = 0;   // broadcast frames delivered to the node
    uint64_t expiredFrameCounter = 0;     // frames dropped at the end of their time to live
    uint64_t unreachableFrameCounter = 0; // frames dropped without a way to their destination
    uint64_t loopedFrameCounter = 0;      // frames dropped that came back to their source
    uint64_t busyFrameCounter = 0;        // frames refused while the next window was full
    uint64_t failedForwardCounter = 0;    // forwarded frames that the next hop failed
    uint64_t outgoingReportCounter = 0;   // sent route failure reports
};

/*
 * Forwarded acknowledged message that waits for the answer of the next hop.
 */
struct ForwardedMessage
{
    bool used = false; // entry holds a message
    Definitions::PortType arrivalPortType
        = Definitions::PortType::UNKNOWN; // port the message arrived on
    uint16_t messageCounter = 0;          // message counter on the link of the next hop
    uint16_t messageType = 0;             // message type of the message
    uint8_t sourceAddress = Definitions::UNASSIGNED_ADDRESS;      // source of the message
    uint8_t destinationAddress = Definitions::UNASSIGNED_ADDRESS; // destination of the message
    uint8_t quoteLength = 0;                                      // number of quoted payload bytes
    std::array<uint8_t, Definitions::MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE>
        quote{}; // first payload bytes of the message
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
     * Deliver a broadcast frame and pass a copy on to the other port while it has links to live.
     * Get the message status of the handler, the copy is never answered.
     */
    [[nodiscard]] Definitions::MessageStatusType handleBroadcastMessage(
        Definitions::PortType portType, const Frame::Frame &frame);

    /*
     * Deliver a frame to the node and get the message status of the handler.
     */
    [[nodiscard]] Definitions::MessageStatusType deliverMessage(const Frame::Frame &frame);

    /*
     * Pass a frame on from its arrival port to the departure port with a route header.
     * Get the answer to the previous hop: accepted when sent, busy when the window is full.
     */
    [[nodiscard]] Definitions::MessageStatusType forwardMessage(
        Definitions::PortType arrivalPortType,
        Definitions::PortType departurePortType,
        const Frame::Frame &frame,
        const Frame::RouteHeader &routeHeader);

    /*
     * Remember a forwarded acknowledged message and its quote until the next hop answers it.
     */
    void rememberForwardedMessage(Definitions::PortType arrivalPortType,
                                  Definitions::PortType departurePortType,
                                  uint16_t messageCounter,
                                  const Frame::Frame &frame,
                                  const Frame::RouteHeader &routeHeader);

    /*
     * Find a forwarded message of a departure port with its message counter and message type.
     * It is null when the message is not a forwarded one.
     */
    [[nodiscard]] ForwardedMessage *getForwardedMessageWithMessageCounter(
        Definitions::PortType departurePortType, uint16_t messageCounter, uint16_t messageType);

    /*
     * Send a route failure report about a forwarded message back on its arrival port to its source.
     * No report is sent about a report or a broadcast, or to a source that cannot receive it.
     */
    void sendRouteFailureReport(const ForwardedMessage &forwardedMessage,
                                Definitions::DeliveryFailureType deliveryFailureType,
                                Definitions::MessageStatusType remoteMessageStatusType);

    /*
     * Drop a frame that has no way to its destination and get the unreachable status.
     */
    [[nodiscard]] Definitions::MessageStatusType dropUnreachableMessage();

    /*
     * Get the link of a port, the link of the primary port for every port but the secondary one.
     */
    [[nodiscard]] Link::Link &getLink(Definitions::PortType portType);
    [[nodiscard]] const Link::Link &getLink(Definitions::PortType portType) const;

    /*
     * Get the forwarded messages of a departure port, of the primary port for any other port.
     */
    [[nodiscard]] std::array<ForwardedMessage, Settings::MaximumPendingFrames> &getForwardedMessages(
        Definitions::PortType departurePortType);

private:
    const RouterConfiguration configuration;    // router configuration
    Link::LinkHandler &handler;                 // reference to handler
    NullTransport::NullTransport nullTransport; // transport of a port without a cable
    RoutePort::RoutePort primaryPort;           // port towards the host
    RoutePort::RoutePort secondaryPort;         // port away from the host
    Link::Link primaryLink;                     // link of the primary port
    Link::Link secondaryLink;                   // link of the secondary port
    const bool secondaryPortUsed;               // secondary port has a transport
    RouterStatistics statistics;                // router statistics
    std::array<ForwardedMessage, Settings::MaximumPendingFrames>
        primaryForwardedMessages; // messages forwarded on the primary port
    std::array<ForwardedMessage, Settings::MaximumPendingFrames>
        secondaryForwardedMessages; // messages forwarded on the secondary port
};

} // namespace sercom::Router

#endif // SERCOM_ROUTER_H

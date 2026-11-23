#include "../include/Router.h"

#include <algorithm>

namespace sercom::Router {

namespace {

/*
 * Get the link configuration of the ports, route capable for a node with an address.
 */
[[nodiscard]] Link::LinkConfiguration getPortLinkConfiguration(
    const Link::LinkConfiguration &linkConfiguration, const RouterConfiguration &routerConfiguration)
{
    // let only a node with an address read and send routed frames
    Link::LinkConfiguration portLinkConfiguration = linkConfiguration;
    portLinkConfiguration.routeCapable = routerConfiguration.nodeAddress
                                         != Definitions::UNASSIGNED_ADDRESS;

    // expose the link configuration of the ports
    return portLinkConfiguration;
}

/*
 * Get the other port of a node with two ports.
 */
[[nodiscard]] Definitions::PortType getOtherPortType(const Definitions::PortType portType)
{
    // expose the primary port as the other port of the secondary one
    if (portType == Definitions::PortType::SECONDARY) {
        return Definitions::PortType::PRIMARY;
    }

    // expose the secondary port as the other port of the primary one
    return Definitions::PortType::SECONDARY;
}

} // namespace

Router::Router(Transport::Transport &primaryTransport,
               Transport::Transport *secondaryTransport,
               Link::LinkHandler &handler,
               const Clock::Clock &clock,
               const Link::LinkConfiguration &linkConfiguration,
               const RouterConfiguration &routerConfiguration)
    : configuration(routerConfiguration)
    , handler(handler)
    , nullTransport()
    , primaryPort(*this, Definitions::PortType::PRIMARY)
    , secondaryPort(*this, Definitions::PortType::SECONDARY)
    , primaryLink(primaryTransport,
                  this->primaryPort,
                  clock,
                  getPortLinkConfiguration(linkConfiguration, routerConfiguration))
    , secondaryLink(secondaryTransport != nullptr
                        ? *secondaryTransport
                        : static_cast<Transport::Transport &>(this->nullTransport),
                    this->secondaryPort,
                    clock,
                    getPortLinkConfiguration(linkConfiguration, routerConfiguration))
    , secondaryPortUsed(secondaryTransport != nullptr)
    , statistics()
    , primaryForwardedMessages()
    , secondaryForwardedMessages()
{}

Router::~Router() {}

Definitions::SendStatusType Router::sendMessage(const uint8_t destinationAddress,
                                                const uint16_t messageType,
                                                const uint8_t *payload,
                                                const size_t payloadLength,
                                                const bool acknowledged,
                                                uint16_t &messageCounter)
{
    // send a message for the neighbour in a direct frame
    if (destinationAddress == Definitions::UNASSIGNED_ADDRESS) {
        return this->primaryLink.sendMessage(messageType,
                                             payload,
                                             payloadLength,
                                             acknowledged,
                                             messageCounter);
    }

    // node must have an address to send to a destination
    if (this->configuration.nodeAddress == Definitions::UNASSIGNED_ADDRESS) {
        return Definitions::SendStatusType::NO_ROUTE;
    }

    // destination must not be the node itself
    if (destinationAddress == this->configuration.nodeAddress) {
        return Definitions::SendStatusType::INVALID_ARGUMENT;
    }

    // send a message for the host in a direct frame to a peer of version 1, the host itself
    const bool primaryPeerVersion1 = this->primaryLink.getPeerVersionType()
                                     == Definitions::PeerVersionType::VERSION_1;
    if (primaryPeerVersion1 && destinationAddress == Definitions::HOST_ADDRESS) {
        return this->primaryLink.sendMessage(messageType,
                                             payload,
                                             payloadLength,
                                             acknowledged,
                                             messageCounter);
    }

    // peer of version 1 must not get a routed frame
    if (primaryPeerVersion1) {
        return Definitions::SendStatusType::NO_ROUTE;
    }

    // send the message in a routed frame with the address and the time to live of the node
    Frame::RouteHeader routeHeader;
    routeHeader.sourceAddress = this->configuration.nodeAddress;
    routeHeader.destinationAddress = destinationAddress;
    routeHeader.timeToLive = this->configuration.timeToLive;
    return this->primaryLink.sendRoutedMessage(routeHeader,
                                               messageType,
                                               payload,
                                               payloadLength,
                                               acknowledged,
                                               messageCounter);
}

void Router::onReceivedData(const Definitions::PortType portType,
                            const uint8_t *data,
                            const size_t size)
{
    // hand the data to the link of the port
    this->getLink(portType).onReceivedData(data, size);
}

void Router::maintain()
{
    // maintain the links of both ports
    this->primaryLink.maintain();
    this->secondaryLink.maintain();
}

bool Router::getSendAcknowledgedAllowed([[maybe_unused]] const uint8_t destinationAddress) const
{
    // expose whether the link of the primary port allows an acknowledged message
    return this->primaryLink.getSendAcknowledgedAllowed();
}

size_t Router::getPendingCount() const
{
    // expose the number of pending messages of the primary port
    return this->primaryLink.getPendingCount();
}

uint8_t Router::getNodeAddress() const
{
    // expose the address of the node
    return this->configuration.nodeAddress;
}

Link::LinkStatistics Router::getLinkStatistics(const Definitions::PortType portType) const
{
    // expose the link statistics of the port
    return this->getLink(portType).getStatistics();
}

Definitions::PeerVersionType Router::getPeerVersionType(const Definitions::PortType portType) const
{
    // expose the protocol version of the peer of the port
    return this->getLink(portType).getPeerVersionType();
}

RouterStatistics Router::getStatistics() const
{
    // expose the router statistics
    return this->statistics;
}

Definitions::MessageStatusType Router::onReceivedPortMessage(const Definitions::PortType portType,
                                                             const Frame::Frame &frame)
{
    // handle a routed frame by the rules of the routed frames
    if (frame.getRouted()) {
        return this->handleRoutedMessage(portType, frame);
    }

    // handle a direct frame
    return this->handleDirectMessage(portType, frame);
}

void Router::onPortDeliveryConfirmed(const Definitions::PortType portType,
                                     const uint16_t messageCounter,
                                     const uint16_t messageType)
{
    // forget a forwarded message that reached the next hop
    ForwardedMessage *forwardedMessage = this->getForwardedMessageWithMessageCounter(portType,
                                                                                     messageCounter,
                                                                                     messageType);
    if (forwardedMessage != nullptr) {
        forwardedMessage->used = false;
        return;
    }

    // own messages leave on the primary port only
    if (portType != Definitions::PortType::PRIMARY) {
        return;
    }

    // notify listeners about the confirmed own message
    this->handler.onDeliveryConfirmed(messageCounter, messageType);
}

void Router::onPortDeliveryFailed(const Definitions::PortType portType,
                                  const uint16_t messageCounter,
                                  const uint16_t messageType,
                                  const Definitions::DeliveryFailureType deliveryFailureType,
                                  const Definitions::MessageStatusType remoteMessageStatusType)
{
    // forwarded message must not have failed at the next hop
    ForwardedMessage *forwardedMessage = this->getForwardedMessageWithMessageCounter(portType,
                                                                                     messageCounter,
                                                                                     messageType);
    if (forwardedMessage != nullptr) {
        // forget the forwarded message
        forwardedMessage->used = false;

        // keep track of failed forwards
        ++this->statistics.failedForwardCounter;

        // do not proceed further
        return;
    }

    // own messages leave on the primary port only
    if (portType != Definitions::PortType::PRIMARY) {
        return;
    }

    // notify listeners about the failed own message
    this->handler.onDeliveryFailed(messageCounter,
                                   messageType,
                                   deliveryFailureType,
                                   remoteMessageStatusType);
}

Definitions::MessageStatusType Router::handleDirectMessage(const Definitions::PortType portType,
                                                           const Frame::Frame &frame)
{
    // deliver a direct frame of the primary port
    if (portType == Definitions::PortType::PRIMARY) {
        return this->deliverMessage(frame);
    }

    // drop every other direct frame
    return this->dropUnreachableMessage();
}

Definitions::MessageStatusType Router::handleRoutedMessage(const Definitions::PortType portType,
                                                           const Frame::Frame &frame)
{
    // node must have an address
    if (this->configuration.nodeAddress == Definitions::UNASSIGNED_ADDRESS) {
        return this->dropUnreachableMessage();
    }

    // frame must come from a node and go to one
    const uint8_t sourceAddress = frame.getSourceAddress();
    const uint8_t destinationAddress = frame.getDestinationAddress();
    if (sourceAddress == Definitions::UNASSIGNED_ADDRESS
        || sourceAddress == Definitions::BROADCAST_ADDRESS
        || destinationAddress == Definitions::UNASSIGNED_ADDRESS) {
        return this->dropUnreachableMessage();
    }

    // frame must not come back to its source
    if (sourceAddress == this->configuration.nodeAddress) {
        // keep track of loops
        ++this->statistics.loopedFrameCounter;

        // do not proceed further
        return Definitions::MessageStatusType::UNREACHABLE;
    }

    // deliver a frame for the node
    if (destinationAddress == this->configuration.nodeAddress) {
        return this->deliverMessage(frame);
    }

    // deliver a broadcast frame and pass it on
    if (destinationAddress == Definitions::BROADCAST_ADDRESS) {
        return this->handleBroadcastMessage(portType, frame);
    }

    // frame for the host must not come from the side of the host
    if (destinationAddress == Definitions::HOST_ADDRESS
        && portType == Definitions::PortType::PRIMARY) {
        return this->dropUnreachableMessage();
    }

    // time to live must allow one more link
    if (frame.getTimeToLive() <= 1) {
        // keep track of expired frames
        ++this->statistics.expiredFrameCounter;

        // do not proceed further
        return Definitions::MessageStatusType::EXPIRED;
    }

    // node must have another port
    const Definitions::PortType otherPortType = getOtherPortType(portType);
    if (otherPortType == Definitions::PortType::SECONDARY && !this->secondaryPortUsed) {
        return this->dropUnreachableMessage();
    }

    // pass the frame on to the other port with one link less to live
    Frame::RouteHeader routeHeader = frame.getRouteHeader();
    --routeHeader.timeToLive;
    return this->forwardMessage(portType, otherPortType, frame, routeHeader);
}

Definitions::MessageStatusType Router::handleBroadcastMessage(const Definitions::PortType portType,
                                                              const Frame::Frame &frame)
{
    // keep track of broadcast frames
    ++this->statistics.broadcastFrameCounter;

    // deliver the broadcast frame to the node
    const Definitions::MessageStatusType messageStatusType = this->handler.onReceivedMessage(frame);

    // time to live must allow a copy for the other port
    if (frame.getTimeToLive() <= 1) {
        return messageStatusType;
    }

    // node must have another port for the copy
    const Definitions::PortType otherPortType = getOtherPortType(portType);
    if (otherPortType == Definitions::PortType::SECONDARY && !this->secondaryPortUsed) {
        return messageStatusType;
    }

    // pass a copy with one link less to live on to the other port at best effort
    Frame::RouteHeader routeHeader = frame.getRouteHeader();
    --routeHeader.timeToLive;
    [[maybe_unused]] const Definitions::MessageStatusType copyMessageStatusType
        = this->forwardMessage(portType, otherPortType, frame, routeHeader);

    // answer with the message status of the node
    return messageStatusType;
}

Definitions::MessageStatusType Router::deliverMessage(const Frame::Frame &frame)
{
    // keep track of local frames
    ++this->statistics.localFrameCounter;

    // notify listeners about the received message
    return this->handler.onReceivedMessage(frame);
}

Definitions::MessageStatusType Router::forwardMessage(const Definitions::PortType arrivalPortType,
                                                      const Definitions::PortType departurePortType,
                                                      const Frame::Frame &frame,
                                                      const Frame::RouteHeader &routeHeader)
{
    // peer of the departure port must read routed frames
    Link::Link &departureLink = this->getLink(departurePortType);
    if (departureLink.getPeerVersionType() == Definitions::PeerVersionType::VERSION_1) {
        return this->dropUnreachableMessage();
    }

    // previous hop must learn that the window of the next link is full
    const bool acknowledged = frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST);
    uint16_t forwardedMessageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = departureLink.sendRoutedMessage(routeHeader,
                                          frame.getMessageType(),
                                          frame.getPayload(),
                                          frame.getPayloadLength(),
                                          acknowledged,
                                          forwardedMessageCounter);
    if (sendStatusType == Definitions::SendStatusType::WINDOW_FULL) {
        // keep track of busy frames
        ++this->statistics.busyFrameCounter;

        // do not proceed further
        return Definitions::MessageStatusType::BUSY;
    }

    // frame must be taken by the next link
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        return this->dropUnreachableMessage();
    }

    // remember an acknowledged frame until the next hop answers it
    if (acknowledged) {
        this->rememberForwardedMessage(arrivalPortType,
                                       departurePortType,
                                       forwardedMessageCounter,
                                       frame.getMessageType());
    }

    // keep track of forwarded frames
    ++this->statistics.forwardedFrameCounter;

    // frame is held by the node now
    return Definitions::MessageStatusType::ACCEPTED;
}

void Router::rememberForwardedMessage(const Definitions::PortType arrivalPortType,
                                      const Definitions::PortType departurePortType,
                                      const uint16_t messageCounter,
                                      const uint16_t messageType)
{
    // free entry must be found, each entry holds a pending message of the departure link
    std::array<ForwardedMessage, Settings::MaximumPendingFrames> &forwardedMessages
        = this->getForwardedMessages(departurePortType);
    const std::array<ForwardedMessage, Settings::MaximumPendingFrames>::iterator &iterator
        = std::find_if(forwardedMessages.begin(),
                       forwardedMessages.end(),
                       [](const ForwardedMessage &forwardedMessage) {
                           return !forwardedMessage.used;
                       });
    if (iterator == forwardedMessages.end()) {
        return;
    }

    // remember the forwarded message
    iterator->used = true;
    iterator->arrivalPortType = arrivalPortType;
    iterator->messageCounter = messageCounter;
    iterator->messageType = messageType;
}

ForwardedMessage *Router::getForwardedMessageWithMessageCounter(
    const Definitions::PortType departurePortType,
    const uint16_t messageCounter,
    const uint16_t messageType)
{
    // forwarded message must be found
    std::array<ForwardedMessage, Settings::MaximumPendingFrames> &forwardedMessages
        = this->getForwardedMessages(departurePortType);
    const std::array<ForwardedMessage, Settings::MaximumPendingFrames>::iterator &iterator
        = std::find_if(forwardedMessages.begin(),
                       forwardedMessages.end(),
                       [messageCounter, messageType](const ForwardedMessage &forwardedMessage) {
                           return forwardedMessage.used
                                  && forwardedMessage.messageCounter == messageCounter
                                  && forwardedMessage.messageType == messageType;
                       });
    if (iterator == forwardedMessages.end()) {
        return nullptr;
    }

    // expose the forwarded message
    return &*iterator;
}

Definitions::MessageStatusType Router::dropUnreachableMessage()
{
    // keep track of unreachable frames
    ++this->statistics.unreachableFrameCounter;

    // expose the unreachable status
    return Definitions::MessageStatusType::UNREACHABLE;
}

Link::Link &Router::getLink(const Definitions::PortType portType)
{
    // expose the link of the secondary port
    if (portType == Definitions::PortType::SECONDARY) {
        return this->secondaryLink;
    }

    // expose the link of the primary port
    return this->primaryLink;
}

const Link::Link &Router::getLink(const Definitions::PortType portType) const
{
    // expose the link of the secondary port
    if (portType == Definitions::PortType::SECONDARY) {
        return this->secondaryLink;
    }

    // expose the link of the primary port
    return this->primaryLink;
}

std::array<ForwardedMessage, Settings::MaximumPendingFrames> &Router::getForwardedMessages(
    const Definitions::PortType departurePortType)
{
    // expose the forwarded messages of the secondary port
    if (departurePortType == Definitions::PortType::SECONDARY) {
        return this->secondaryForwardedMessages;
    }

    // expose the forwarded messages of the primary port
    return this->primaryForwardedMessages;
}

} // namespace sercom::Router

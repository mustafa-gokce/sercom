#include "../include/Router.h"

#include <algorithm>

#include "../include/ByteCodec.h"

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
    , proxyReportAddress(Definitions::HOST_ADDRESS)
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
        const ForwardedMessage failedMessage = *forwardedMessage;
        forwardedMessage->used = false;

        // keep track of failed forwards
        ++this->statistics.failedForwardCounter;

        // report the failed forward to the source of the message
        this->sendRouteFailureReport(failedMessage, deliveryFailureType, remoteMessageStatusType);

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

    // drop a direct frame of the secondary port without a neighbour to stand in for
    if (this->configuration.proxyAddress == Definitions::UNASSIGNED_ADDRESS) {
        return this->dropUnreachableMessage();
    }

    // pass the frame of the neighbour up as a routed frame of the proxy address
    Frame::RouteHeader routeHeader;
    routeHeader.sourceAddress = this->configuration.proxyAddress;
    routeHeader.destinationAddress = this->proxyReportAddress;
    routeHeader.timeToLive = this->configuration.timeToLive;
    ++this->statistics.proxiedFrameCounter;
    return this->forwardMessage(portType, Definitions::PortType::PRIMARY, frame, routeHeader, true);
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

    // send a frame for the neighbour without an address down in a direct frame
    const bool otherPeerVersion2 = this->getLink(otherPortType).getPeerVersionType()
                                   == Definitions::PeerVersionType::VERSION_2;
    if (otherPortType == Definitions::PortType::SECONDARY
        && destinationAddress == this->configuration.proxyAddress && !otherPeerVersion2) {
        this->proxyReportAddress = sourceAddress;
        ++this->statistics.proxiedFrameCounter;
        return this->forwardMessage(portType, otherPortType, frame, frame.getRouteHeader(), false);
    }

    // pass the frame on to the other port with one link less to live
    Frame::RouteHeader routeHeader = frame.getRouteHeader();
    --routeHeader.timeToLive;
    return this->forwardMessage(portType, otherPortType, frame, routeHeader, true);
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
        = this->forwardMessage(portType, otherPortType, frame, routeHeader, true);

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
                                                      const Frame::RouteHeader &routeHeader,
                                                      const bool routed)
{
    // peer of the departure port must read a routed frame
    Link::Link &departureLink = this->getLink(departurePortType);
    if (routed && departureLink.getPeerVersionType() == Definitions::PeerVersionType::VERSION_1) {
        return this->dropUnreachableMessage();
    }

    // send the frame routed or direct on the link of the departure port
    const bool acknowledged = frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST);
    uint16_t forwardedMessageCounter = 0;
    Definitions::SendStatusType sendStatusType = Definitions::SendStatusType::UNKNOWN;
    if (routed) {
        sendStatusType = departureLink.sendRoutedMessage(routeHeader,
                                                         frame.getMessageType(),
                                                         frame.getPayload(),
                                                         frame.getPayloadLength(),
                                                         acknowledged,
                                                         forwardedMessageCounter);
    } else {
        sendStatusType = departureLink.sendMessage(frame.getMessageType(),
                                                   frame.getPayload(),
                                                   frame.getPayloadLength(),
                                                   acknowledged,
                                                   forwardedMessageCounter);
    }

    // previous hop must learn that the window of the next link is full
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
                                       frame,
                                       routeHeader);
    }

    // keep track of forwarded frames
    ++this->statistics.forwardedFrameCounter;

    // frame is held by the node now
    return Definitions::MessageStatusType::ACCEPTED;
}

void Router::rememberForwardedMessage(const Definitions::PortType arrivalPortType,
                                      const Definitions::PortType departurePortType,
                                      const uint16_t messageCounter,
                                      const Frame::Frame &frame,
                                      const Frame::RouteHeader &routeHeader)
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

    // remember the forwarded message with its route
    iterator->used = true;
    iterator->arrivalPortType = arrivalPortType;
    iterator->messageCounter = messageCounter;
    iterator->messageType = frame.getMessageType();
    iterator->sourceAddress = routeHeader.sourceAddress;
    iterator->destinationAddress = routeHeader.destinationAddress;

    // quote the first payload bytes
    const size_t quoteLength = std::min(static_cast<size_t>(frame.getPayloadLength()),
                                        Definitions::MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE);
    std::copy_n(frame.getPayload(), quoteLength, iterator->quote.begin());
    iterator->quoteLength = static_cast<uint8_t>(quoteLength);
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

void Router::sendRouteFailureReport(const ForwardedMessage &forwardedMessage,
                                    const Definitions::DeliveryFailureType deliveryFailureType,
                                    const Definitions::MessageStatusType remoteMessageStatusType)
{
    // failed message must be neither a report nor a broadcast
    if (forwardedMessage.messageType == Definitions::ROUTE_FAILURE_REPORT_MESSAGE_TYPE
        || forwardedMessage.destinationAddress == Definitions::BROADCAST_ADDRESS) {
        return;
    }

    // source must be a node that can receive the report
    if (forwardedMessage.sourceAddress == Definitions::UNASSIGNED_ADDRESS
        || forwardedMessage.sourceAddress == Definitions::BROADCAST_ADDRESS
        || forwardedMessage.sourceAddress == this->configuration.proxyAddress) {
        return;
    }

    // peer of the arrival port must read routed frames
    Link::Link &arrivalLink = this->getLink(forwardedMessage.arrivalPortType);
    if (arrivalLink.getPeerVersionType() == Definitions::PeerVersionType::VERSION_1) {
        return;
    }

    // write the route failure report with the quote of the failed message
    static constexpr size_t maximumReportSize = Definitions::ROUTE_FAILURE_REPORT_SIZE
                                                + Definitions::MAXIMUM_ROUTE_FAILURE_QUOTE_SIZE;
    std::array<uint8_t, maximumReportSize> report = std::array<uint8_t, maximumReportSize>();
    ByteCodec::ByteWriter byteWriter(report.data(), report.size());
    byteWriter.writeUint8(forwardedMessage.destinationAddress);
    byteWriter.writeUint16(forwardedMessage.messageType);
    byteWriter.writeUint8(static_cast<uint8_t>(deliveryFailureType));
    byteWriter.writeUint8(Definitions::fromMessageStatusType(remoteMessageStatusType));
    byteWriter.writeUint8(forwardedMessage.quoteLength);
    byteWriter.writeBytes(forwardedMessage.quote.data(), forwardedMessage.quoteLength);

    // route failure report must be sent to the source without acknowledgement
    Frame::RouteHeader routeHeader;
    routeHeader.sourceAddress = this->configuration.nodeAddress;
    routeHeader.destinationAddress = forwardedMessage.sourceAddress;
    routeHeader.timeToLive = this->configuration.timeToLive;
    uint16_t messageCounter = 0;
    const Definitions::SendStatusType sendStatusType
        = arrivalLink.sendRoutedMessage(routeHeader,
                                        Definitions::ROUTE_FAILURE_REPORT_MESSAGE_TYPE,
                                        report.data(),
                                        byteWriter.getSize(),
                                        false,
                                        messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        return;
    }

    // keep track of route failure reports
    ++this->statistics.outgoingReportCounter;
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

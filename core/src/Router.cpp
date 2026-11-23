#include "../include/Router.h"

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
    , statistics()
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

Definitions::MessageStatusType Router::handleRoutedMessage(
    [[maybe_unused]] const Definitions::PortType portType, const Frame::Frame &frame)
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

    // drop a frame for another node
    return this->dropUnreachableMessage();
}

Definitions::MessageStatusType Router::deliverMessage(const Frame::Frame &frame)
{
    // keep track of local frames
    ++this->statistics.localFrameCounter;

    // notify listeners about the received message
    return this->handler.onReceivedMessage(frame);
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

} // namespace sercom::Router

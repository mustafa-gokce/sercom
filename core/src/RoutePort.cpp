#include "../include/RoutePort.h"

namespace sercom::RoutePort {

RoutePortHandler::~RoutePortHandler() {}

RoutePort::RoutePort(RoutePortHandler &handler, const Definitions::PortType portType)
    : handler(handler)
    , portType(portType)
{}

RoutePort::~RoutePort() {}

Definitions::PortType RoutePort::getPortType() const
{
    // expose the port type
    return this->portType;
}

Definitions::MessageStatusType RoutePort::onReceivedMessage(const Frame::Frame &frame)
{
    // notify listeners about the received message of the port
    return this->handler.onReceivedPortMessage(this->portType, frame);
}

void RoutePort::onDeliveryConfirmed(const uint16_t messageCounter, const uint16_t messageType)
{
    // notify listeners about the confirmed delivery of the port
    this->handler.onPortDeliveryConfirmed(this->portType, messageCounter, messageType);
}

void RoutePort::onDeliveryFailed(const uint16_t messageCounter,
                                 const uint16_t messageType,
                                 const Definitions::DeliveryFailureType deliveryFailureType,
                                 const Definitions::MessageStatusType remoteMessageStatusType)
{
    // notify listeners about the failed delivery of the port
    this->handler.onPortDeliveryFailed(this->portType,
                                       messageCounter,
                                       messageType,
                                       deliveryFailureType,
                                       remoteMessageStatusType);
}

} // namespace sercom::RoutePort

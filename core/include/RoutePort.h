#ifndef SERCOM_ROUTE_PORT_H
#define SERCOM_ROUTE_PORT_H

#include <cstdint>

#include "../include/Definitions.h"
#include "../include/Frame.h"
#include "../include/Link.h"

namespace sercom::RoutePort {

class RoutePortHandler
{
public:
    /*
     * Receiver of the link events of the ports of a node, implemented by the router.
     */
    RoutePortHandler() = default;
    virtual ~RoutePortHandler();

    // delete copy constructor and copy assignment operator
    RoutePortHandler(const RoutePortHandler &) = delete;
    RoutePortHandler &operator=(const RoutePortHandler &) = delete;

    // delete move constructor and move assignment operator
    RoutePortHandler(RoutePortHandler &&) = delete;
    RoutePortHandler &operator=(RoutePortHandler &&) = delete;

public:
    /*
     * Listen for a received message of a port and get the message status that answers a requested ACK.
     * The frame reference is valid during the call only.
     */
    [[nodiscard]] virtual Definitions::MessageStatusType onReceivedPortMessage(
        Definitions::PortType portType, const Frame::Frame &frame)
        = 0;

    /*
     * Listen for an acknowledged message of a port that reached the peer.
     * The slot of the message is free again during the call.
     */
    virtual void onPortDeliveryConfirmed(Definitions::PortType portType,
                                         uint16_t messageCounter,
                                         uint16_t messageType)
        = 0;

    /*
     * Listen for an acknowledged message of a port that timed out or was rejected by the peer.
     * The slot of the message is free again during the call.
     */
    virtual void onPortDeliveryFailed(Definitions::PortType portType,
                                      uint16_t messageCounter,
                                      uint16_t messageType,
                                      Definitions::DeliveryFailureType deliveryFailureType,
                                      Definitions::MessageStatusType remoteMessageStatusType)
        = 0;
};

class RoutePort final : public Link::LinkHandler
{
public:
    /*
     * Port of a node that hands every event of its link to the route port handler with its port type.
     */
    explicit RoutePort(RoutePortHandler &handler, Definitions::PortType portType);
    ~RoutePort() final;

    // delete copy constructor and copy assignment operator
    RoutePort(const RoutePort &) = delete;
    RoutePort &operator=(const RoutePort &) = delete;

    // delete move constructor and move assignment operator
    RoutePort(RoutePort &&) = delete;
    RoutePort &operator=(RoutePort &&) = delete;

public:
    /*
     * Get the port type.
     */
    [[nodiscard]] Definitions::PortType getPortType() const;

private:
    /*
     * Listen for a received message and hand it to the handler with the port type.
     */
    [[nodiscard]] Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame) final;

    /*
     * Listen for a confirmed delivery and hand it to the handler with the port type.
     */
    void onDeliveryConfirmed(uint16_t messageCounter, uint16_t messageType) final;

    /*
     * Listen for a failed delivery and hand it to the handler with the port type.
     */
    void onDeliveryFailed(uint16_t messageCounter,
                          uint16_t messageType,
                          Definitions::DeliveryFailureType deliveryFailureType,
                          Definitions::MessageStatusType remoteMessageStatusType) final;

private:
    RoutePortHandler &handler;            // reference to handler
    const Definitions::PortType portType; // port type of the port
};

} // namespace sercom::RoutePort

#endif // SERCOM_ROUTE_PORT_H

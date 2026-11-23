#include "../include/DeviceClient.h"

#include <algorithm>

#include "../../core/include/Settings.h"

namespace sercom::DeviceClient {

DeviceClientHandler::~DeviceClientHandler() {}

DeviceClient::DeviceClient(Transport::Transport &transport,
                           const Clock::Clock &clock,
                           DeviceClientHandler &handler,
                           const Link::LinkConfiguration &linkConfiguration,
                           const Router::RouterConfiguration &routerConfiguration)
    : clock(clock)
    , handler(handler)
    , router(transport, nullptr, *this, clock, linkConfiguration, routerConfiguration)
    , destinationAddress(Definitions::UNASSIGNED_ADDRESS)
    , nextPendingPingIndex(0)
    , ignoredPingResponseCounter(0)
    , pendingPings()
{}

DeviceClient::~DeviceClient() {}

Definitions::SendStatusType DeviceClient::sendPingRequest(uint16_t &messageCounter)
{
    // create ping request with the current time
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    const Messages::PingRequest pingRequest(currentTime);

    // ping request must be sent
    const Definitions::SendStatusType sendStatusType
        = this->sendRequest(Messages::MessageType::PING_REQUEST,
                            pingRequest.getPayload(),
                            pingRequest.getPayloadLength(),
                            messageCounter);
    if (sendStatusType != Definitions::SendStatusType::SENT) {
        return sendStatusType;
    }

    // remember the ping request for its ping response
    this->rememberPendingPing(currentTime);

    // ping request sent successfully
    return Definitions::SendStatusType::SENT;
}

Definitions::SendStatusType DeviceClient::sendDeviceInfoRequest(uint16_t &messageCounter)
{
    // create device info request
    const Messages::DeviceInfoRequest deviceInfoRequest;

    // send the device info request
    return this->sendRequest(Messages::MessageType::DEVICE_INFO_REQUEST,
                             deviceInfoRequest.getPayload(),
                             deviceInfoRequest.getPayloadLength(),
                             messageCounter);
}

Definitions::SendStatusType DeviceClient::sendSetLedRequest(const bool ledOn,
                                                            uint16_t &messageCounter)
{
    // create set LED request
    const Messages::SetLedRequest setLedRequest(ledOn);

    // send the set LED request
    return this->sendRequest(Messages::MessageType::SET_LED_REQUEST,
                             setLedRequest.getPayload(),
                             setLedRequest.getPayloadLength(),
                             messageCounter);
}

Definitions::SendStatusType DeviceClient::sendSetTelemetryPeriodRequest(
    const std::chrono::milliseconds telemetryPeriod, uint16_t &messageCounter)
{
    // telemetry period must fit into the set telemetry period request
    const Messages::SetTelemetryPeriodRequest setTelemetryPeriodRequest(telemetryPeriod);
    if (!setTelemetryPeriodRequest.getLikelyValid()) {
        return Definitions::SendStatusType::INVALID_ARGUMENT;
    }

    // send the set telemetry period request
    return this->sendRequest(Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST,
                             setTelemetryPeriodRequest.getPayload(),
                             setTelemetryPeriodRequest.getPayloadLength(),
                             messageCounter);
}

Definitions::SendStatusType DeviceClient::sendEchoRequest(const uint8_t *data,
                                                          const size_t size,
                                                          uint16_t &messageCounter)
{
    // data must fit into a payload of this build
    if (size > Settings::MaximumPayloadSize) {
        return Definitions::SendStatusType::PAYLOAD_TOO_LONG;
    }

    // data must exist when it is not empty
    if (size > 0 && data == nullptr) {
        return Definitions::SendStatusType::INVALID_ARGUMENT;
    }

    // create echo request
    const Messages::EchoRequest echoRequest(data, size);

    // send the echo request
    return this->sendRequest(Messages::MessageType::ECHO_REQUEST,
                             echoRequest.getPayload(),
                             echoRequest.getPayloadLength(),
                             messageCounter);
}

void DeviceClient::onReceivedData(const uint8_t *data, const size_t size)
{
    // hand the data to the router
    this->router.onReceivedData(Definitions::PortType::PRIMARY, data, size);
}

void DeviceClient::maintain()
{
    // maintain the router
    this->router.maintain();
}

uint8_t DeviceClient::getDestinationAddress() const
{
    // expose the destination address
    return this->destinationAddress;
}

void DeviceClient::setDestinationAddress(const uint8_t destinationAddress)
{
    // set the destination address
    this->destinationAddress = destinationAddress;
}

bool DeviceClient::getSendRequestAllowed() const
{
    // expose whether the router allows an acknowledged message to the destination
    return this->router.getSendAcknowledgedAllowed(this->destinationAddress);
}

size_t DeviceClient::getPendingCount() const
{
    // expose the number of pending messages of the router
    return this->router.getPendingCount();
}

Link::LinkStatistics DeviceClient::getLinkStatistics() const
{
    // expose the link statistics
    return this->router.getLinkStatistics(Definitions::PortType::PRIMARY);
}

Router::RouterStatistics DeviceClient::getRouterStatistics() const
{
    // expose the router statistics
    return this->router.getStatistics();
}

uint64_t DeviceClient::getIgnoredPingResponseCounter() const
{
    // expose the number of ignored ping responses
    return this->ignoredPingResponseCounter;
}

Definitions::MessageStatusType DeviceClient::onReceivedMessage(const Frame::Frame &frame)
{
    // handle the message by its message type
    switch (Messages::toMessageType(frame.getMessageType())) {
    // handle a ping response
    case Messages::MessageType::PING_RESPONSE: {
        return this->handlePingResponse(frame);
    }

    // handle a device info response
    case Messages::MessageType::DEVICE_INFO_RESPONSE: {
        return this->handleDeviceInfoResponse(frame);
    }

    // handle a set LED response
    case Messages::MessageType::SET_LED_RESPONSE: {
        return this->handleSetLedResponse(frame);
    }

    // handle a telemetry report response
    case Messages::MessageType::TELEMETRY_REPORT_RESPONSE: {
        return this->handleTelemetryReportResponse(frame);
    }

    // handle a log report response
    case Messages::MessageType::LOG_REPORT_RESPONSE: {
        return this->handleLogReportResponse(frame);
    }

    // handle an echo response
    case Messages::MessageType::ECHO_RESPONSE: {
        return this->handleEchoResponse(frame);
    }

    // do not handle requests and unknown messages
    case Messages::MessageType::PING_REQUEST:
    case Messages::MessageType::DEVICE_INFO_REQUEST:
    case Messages::MessageType::ECHO_REQUEST:
    case Messages::MessageType::SET_LED_REQUEST:
    case Messages::MessageType::SET_TELEMETRY_PERIOD_REQUEST:
    case Messages::MessageType::UNKNOWN:
    default: {
        return Definitions::MessageStatusType::UNSUPPORTED_MESSAGE_TYPE;
    }
    }
}

void DeviceClient::onDeliveryConfirmed(const uint16_t messageCounter, const uint16_t messageType)
{
    // notify listeners about the confirmed request
    this->handler.onRequestConfirmed(messageCounter, Messages::toMessageType(messageType));
}

void DeviceClient::onDeliveryFailed(const uint16_t messageCounter,
                                    const uint16_t messageType,
                                    const Definitions::DeliveryFailureType deliveryFailureType,
                                    const Definitions::MessageStatusType remoteMessageStatusType)
{
    // notify listeners about the failed request
    this->handler.onRequestFailed(messageCounter,
                                  Messages::toMessageType(messageType),
                                  deliveryFailureType,
                                  remoteMessageStatusType);
}

Definitions::MessageStatusType DeviceClient::handlePingResponse(const Frame::Frame &frame)
{
    // get ping response arguments
    const Messages::PingResponse pingResponse(frame);
    if (!pingResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // pending ping of the ping response must be found
    const std::array<PendingPing, AppSettings::PING_HISTORY_SIZE>::iterator &iterator
        = std::find_if(this->pendingPings.begin(),
                       this->pendingPings.end(),
                       [&pingResponse](const PendingPing &pendingPing) {
                           return pendingPing.used
                                  && static_cast<uint32_t>(pendingPing.requestTimestamp.count())
                                         == static_cast<uint32_t>(
                                             pingResponse.getRequestTimestamp().count());
                       });
    if (iterator == this->pendingPings.end()) {
        // keep track of ignored ping responses
        ++this->ignoredPingResponseCounter;

        // do not proceed further
        return Definitions::MessageStatusType::ACCEPTED;
    }

    // forget the answered ping request unless every node answers it
    iterator->used = iterator->broadcast;

    // notify listeners about the ping response and its round trip time
    this->handler.onReceivedPingResponse(pingResponse,
                                         this->clock.getTime() - iterator->requestTimestamp);

    // ping response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::MessageStatusType DeviceClient::handleDeviceInfoResponse(const Frame::Frame &frame)
{
    // get device info response arguments
    const Messages::DeviceInfoResponse deviceInfoResponse(frame);
    if (!deviceInfoResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // notify listeners about the device info response
    this->handler.onReceivedDeviceInfoResponse(deviceInfoResponse);

    // device info response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::MessageStatusType DeviceClient::handleSetLedResponse(const Frame::Frame &frame)
{
    // get set LED response arguments
    const Messages::SetLedResponse setLedResponse(frame);
    if (!setLedResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // notify listeners about the set LED response
    this->handler.onReceivedSetLedResponse(setLedResponse);

    // set LED response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::MessageStatusType DeviceClient::handleTelemetryReportResponse(const Frame::Frame &frame)
{
    // get telemetry report response arguments
    const Messages::TelemetryReportResponse telemetryReportResponse(frame);
    if (!telemetryReportResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // notify listeners about the telemetry report response
    this->handler.onReceivedTelemetryReportResponse(telemetryReportResponse);

    // telemetry report response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::MessageStatusType DeviceClient::handleLogReportResponse(const Frame::Frame &frame)
{
    // get log report response arguments
    const Messages::LogReportResponse logReportResponse(frame);
    if (!logReportResponse.getLikelyValid()) {
        return Definitions::MessageStatusType::MALFORMED_PAYLOAD;
    }

    // notify listeners about the log report response
    this->handler.onReceivedLogReportResponse(logReportResponse);

    // log report response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::MessageStatusType DeviceClient::handleEchoResponse(const Frame::Frame &frame)
{
    // get echo response arguments
    const Messages::EchoResponse echoResponse(frame);

    // notify listeners about the echo response
    this->handler.onReceivedEchoResponse(echoResponse);

    // echo response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::SendStatusType DeviceClient::sendRequest(const Messages::MessageType messageType,
                                                      const uint8_t *payload,
                                                      const size_t payloadLength,
                                                      uint16_t &messageCounter)
{
    // send the request acknowledged to the destination
    return this->router.sendMessage(this->destinationAddress,
                                    Messages::fromMessageType(messageType),
                                    payload,
                                    payloadLength,
                                    true,
                                    messageCounter);
}

void DeviceClient::rememberPendingPing(const std::chrono::milliseconds requestTimestamp)
{
    // overwrite the oldest pending ping
    PendingPing &pendingPing = this->pendingPings[this->nextPendingPingIndex];
    pendingPing.used = true;
    pendingPing.requestTimestamp = requestTimestamp;
    pendingPing.broadcast = this->destinationAddress == Definitions::BROADCAST_ADDRESS;

    // move to the next pending ping of the ring
    this->nextPendingPingIndex = (this->nextPendingPingIndex + 1) % this->pendingPings.size();
}

} // namespace sercom::DeviceClient

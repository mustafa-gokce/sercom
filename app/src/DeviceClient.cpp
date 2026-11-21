#include "../include/DeviceClient.h"

#include <algorithm>

namespace sercom::DeviceClient {

DeviceClientHandler::~DeviceClientHandler() {}

DeviceClient::DeviceClient(Transport::Transport &transport,
                           const Clock::Clock &clock,
                           DeviceClientHandler &handler,
                           const Link::LinkConfiguration &linkConfiguration)
    : clock(clock)
    , handler(handler)
    , link(transport, *this, clock, linkConfiguration)
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

void DeviceClient::onReceivedData(const uint8_t *data, const size_t size)
{
    // hand the data to the link
    this->link.onReceivedData(data, size);
}

void DeviceClient::maintain()
{
    // maintain the link
    this->link.maintain();
}

bool DeviceClient::getSendRequestAllowed() const
{
    // expose whether the link allows an acknowledged message
    return this->link.getSendAcknowledgedAllowed();
}

size_t DeviceClient::getPendingCount() const
{
    // expose the number of pending messages of the link
    return this->link.getPendingCount();
}

Link::LinkStatistics DeviceClient::getLinkStatistics() const
{
    // expose the link statistics
    return this->link.getStatistics();
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

    // forget the answered ping request
    iterator->used = false;

    // notify listeners about the ping response and its round trip time
    this->handler.onReceivedPingResponse(pingResponse,
                                         this->clock.getTime() - iterator->requestTimestamp);

    // ping response handled successfully
    return Definitions::MessageStatusType::ACCEPTED;
}

Definitions::SendStatusType DeviceClient::sendRequest(const Messages::MessageType messageType,
                                                      const uint8_t *payload,
                                                      const size_t payloadLength,
                                                      uint16_t &messageCounter)
{
    // send the request acknowledged
    return this->link.sendMessage(Messages::fromMessageType(messageType),
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

    // move to the next pending ping of the ring
    this->nextPendingPingIndex = (this->nextPendingPingIndex + 1) % this->pendingPings.size();
}

} // namespace sercom::DeviceClient

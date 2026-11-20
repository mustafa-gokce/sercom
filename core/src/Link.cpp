#include "../include/Link.h"

#include <algorithm>

namespace sercom::Link {

LinkHandler::~LinkHandler() {}

Link::Link(Transport::Transport &transport,
           LinkHandler &handler,
           const Clock::Clock &clock,
           const LinkConfiguration &configuration)
    : transport(transport)
    , handler(handler)
    , clock(clock)
    , configuration(configuration)
    , frameDecoder(*this, configuration.interByteTimeout)
    , nextReceivedMessageIndex(0)
    , nextMessageCounter(0)
    , statistics()
    , pendingMessages()
    , receivedMessages()
{}

Link::~Link() {}

Definitions::SendStatusType Link::sendMessage(const uint16_t messageType,
                                              const uint8_t *payload,
                                              const size_t payloadLength,
                                              const bool acknowledged,
                                              uint16_t &messageCounter)
{
    // payload must fit into a frame of this build
    if (payloadLength > Settings::MaximumPayloadSize) {
        return Definitions::SendStatusType::PAYLOAD_TOO_LONG;
    }

    // payload must exist when it is not empty
    if (payloadLength > 0 && payload == nullptr) {
        return Definitions::SendStatusType::INVALID_ARGUMENT;
    }

    // acknowledged message must get a free pending message
    const std::array<PendingMessage, Settings::MaximumPendingFrames>::iterator &iterator
        = std::find_if(this->pendingMessages.begin(),
                       this->pendingMessages.end(),
                       [](const PendingMessage &pendingMessage) { return !pendingMessage.used; });
    if (acknowledged && iterator == this->pendingMessages.end()) {
        return Definitions::SendStatusType::WINDOW_FULL;
    }

    // assign the message counter that wraps from 65535 to 0
    const uint16_t assignedMessageCounter = this->nextMessageCounter;
    ++this->nextMessageCounter;

    // frame must be sent
    const uint8_t flags = acknowledged ? Definitions::fromFrameFlagType(
                                             Definitions::FrameFlagType::ACK_REQUEST)
                                       : static_cast<uint8_t>(0);
    const Frame::Frame frame(flags, assignedMessageCounter, messageType, payload, payloadLength);
    if (!this->sendFrame(frame)) {
        return Definitions::SendStatusType::TRANSPORT_ERROR;
    }

    // keep a copy for retransmissions
    if (acknowledged) {
        iterator->used = true;
        iterator->messageCounter = assignedMessageCounter;
        iterator->messageType = messageType;
        iterator->payloadLength = static_cast<uint8_t>(payloadLength);
        iterator->retransmissionCount = 0;
        iterator->lastSentTimestamp = this->clock.getTime();
        std::copy_n(payload, payloadLength, iterator->payload.begin());
    }

    // store the message counter
    messageCounter = assignedMessageCounter;

    // message sent successfully
    return Definitions::SendStatusType::SENT;
}

void Link::onReceivedData(const uint8_t *data, const size_t size)
{
    // hand the bytes to the decoder
    this->frameDecoder.onReceivedData(data, size, this->clock.getTime());
}

void Link::maintain()
{
    // get the current time
    const std::chrono::milliseconds currentTime = this->clock.getTime();

    // drop a partial frame of a silent line
    this->frameDecoder.checkTimeout(currentTime);

    // iterate over the pending messages
    std::for_each(this->pendingMessages.begin(),
                  this->pendingMessages.end(),
                  [this, currentTime](PendingMessage &pendingMessage) {
                      // slot must hold a message
                      if (!pendingMessage.used) {
                          return;
                      }

                      // retransmit timeout must have passed
                      if (currentTime - pendingMessage.lastSentTimestamp
                          < this->configuration.retransmitTimeout) {
                          return;
                      }

                      // give up after the last retransmission
                      if (pendingMessage.retransmissionCount
                          >= this->configuration.maximumRetransmissions) {
                          // free the slot
                          pendingMessage.used = false;

                          // keep track of failed deliveries
                          ++this->statistics.failedDeliveryCounter;

                          // notify listeners about the failed delivery
                          this->handler.onDeliveryFailed(pendingMessage.messageCounter,
                                                         pendingMessage.messageType,
                                                         Definitions::DeliveryFailureType::TIMED_OUT,
                                                         Definitions::MessageStatusType::UNKNOWN);

                          // do not proceed further
                          return;
                      }

                      // keep track of retransmissions
                      ++pendingMessage.retransmissionCount;
                      pendingMessage.lastSentTimestamp = currentTime;
                      ++this->statistics.retransmissionCounter;

                      // send the message again with the retransmission flag
                      const uint8_t flags = static_cast<uint8_t>(
                          Definitions::fromFrameFlagType(Definitions::FrameFlagType::ACK_REQUEST)
                          | Definitions::fromFrameFlagType(
                              Definitions::FrameFlagType::RETRANSMISSION));
                      const Frame::Frame frame(flags,
                                               pendingMessage.messageCounter,
                                               pendingMessage.messageType,
                                               pendingMessage.payload.data(),
                                               pendingMessage.payloadLength);
                      [[maybe_unused]] const bool frameSent = this->sendFrame(frame);
                  });
}

bool Link::getSendAcknowledgedAllowed() const
{
    // expose whether the window has a free slot
    return this->getPendingCount() < this->pendingMessages.size();
}

size_t Link::getPendingCount() const
{
    // expose the number of used slots
    return static_cast<size_t>(
        std::count_if(this->pendingMessages.cbegin(),
                      this->pendingMessages.cend(),
                      [](const PendingMessage &pendingMessage) { return pendingMessage.used; }));
}

uint16_t Link::getNextMessageCounter() const
{
    // expose the message counter of the next message
    return this->nextMessageCounter;
}

LinkStatistics Link::getStatistics() const
{
    // add the decoder statistics
    LinkStatistics currentStatistics = this->statistics;
    currentStatistics.decoderStatistics = this->frameDecoder.getStatistics();

    // expose the link statistics
    return currentStatistics;
}

void Link::onReceivedFrame(const Frame::Frame &frame)
{
    // keep track of frames
    ++this->statistics.incomingFrameCounter;

    // match an answer to its pending message
    if (frame.getFlag(Definitions::FrameFlagType::ACK)
        || frame.getFlag(Definitions::FrameFlagType::NACK)) {
        this->handleAcknowledgement(frame);
        return;
    }

    // deliver a fire and forget message without an answer
    if (!frame.getFlag(Definitions::FrameFlagType::ACK_REQUEST)) {
        // keep track of messages
        ++this->statistics.deliveredMessageCounter;

        // notify listeners about the received message
        [[maybe_unused]] const Definitions::MessageStatusType messageStatusType
            = this->handler.onReceivedMessage(frame);

        // do not proceed further
        return;
    }

    // message must not be a retransmission of a delivered message
    const ReceivedMessage *receivedMessage
        = frame.getFlag(Definitions::FrameFlagType::RETRANSMISSION)
              ? this->getReceivedMessageWithMessageCounter(frame.getMessageCounter(),
                                                           frame.getMessageType())
              : nullptr;
    if (receivedMessage != nullptr) {
        // keep track of duplicate frames
        ++this->statistics.duplicateFrameCounter;

        // answer with the remembered message status
        this->sendAcknowledgement(frame, receivedMessage->messageStatusType);

        // do not proceed further
        return;
    }

    // keep track of messages
    ++this->statistics.deliveredMessageCounter;

    // notify listeners about the received message
    const Definitions::MessageStatusType messageStatusType = this->handler.onReceivedMessage(frame);

    // answer with the message status of the application
    this->rememberReceivedMessage(frame.getMessageCounter(),
                                  frame.getMessageType(),
                                  messageStatusType);
    this->sendAcknowledgement(frame, messageStatusType);
}

void Link::onDecodeError([[maybe_unused]] const Definitions::DecodeErrorType decodeErrorType) {}

bool Link::sendFrame(const Frame::Frame &frame)
{
    // frame must be valid
    if (!frame.getLikelyValid()) {
        // keep track of transport errors
        ++this->statistics.transportErrorCounter;

        // do not proceed further
        return false;
    }

    // transport must take every byte
    if (!this->transport.sendData(frame.getData(), frame.getSize())) {
        // keep track of transport errors
        ++this->statistics.transportErrorCounter;

        // do not proceed further
        return false;
    }

    // keep track of frames
    ++this->statistics.outgoingFrameCounter;

    // frame sent successfully
    return true;
}

void Link::sendAcknowledgement(const Frame::Frame &frame,
                               const Definitions::MessageStatusType messageStatusType)
{
    // answer an accepted message with an empty ACK
    if (messageStatusType == Definitions::MessageStatusType::ACCEPTED) {
        // acknowledgement frame must be sent
        const Frame::Frame acknowledgementFrame(Definitions::fromFrameFlagType(
                                                    Definitions::FrameFlagType::ACK),
                                                frame.getMessageCounter(),
                                                frame.getMessageType(),
                                                nullptr,
                                                0);
        if (!this->sendFrame(acknowledgementFrame)) {
            return;
        }

        // keep track of acknowledgements
        ++this->statistics.outgoingAcknowledgementCounter;

        // do not proceed further
        return;
    }

    // rejection frame with the message status must be sent
    const uint8_t messageStatusTypeAsInteger = Definitions::fromMessageStatusType(messageStatusType);
    const Frame::Frame rejectionFrame(Definitions::fromFrameFlagType(
                                          Definitions::FrameFlagType::NACK),
                                      frame.getMessageCounter(),
                                      frame.getMessageType(),
                                      &messageStatusTypeAsInteger,
                                      sizeof(messageStatusTypeAsInteger));
    if (!this->sendFrame(rejectionFrame)) {
        return;
    }

    // keep track of rejections
    ++this->statistics.outgoingRejectionCounter;
}

void Link::handleAcknowledgement(const Frame::Frame &frame)
{
    // pending message of the answer must be found
    const std::array<PendingMessage, Settings::MaximumPendingFrames>::iterator &iterator
        = std::find_if(this->pendingMessages.begin(),
                       this->pendingMessages.end(),
                       [&frame](const PendingMessage &pendingMessage) {
                           return pendingMessage.used
                                  && pendingMessage.messageCounter == frame.getMessageCounter()
                                  && pendingMessage.messageType == frame.getMessageType();
                       });
    if (iterator == this->pendingMessages.end()) {
        // keep track of unexpected acknowledgements
        ++this->statistics.unexpectedAcknowledgementCounter;

        // do not proceed further
        return;
    }

    // free the slot of the pending message
    iterator->used = false;

    // confirm the delivery on an ACK frame
    if (frame.getFlag(Definitions::FrameFlagType::ACK)) {
        // keep track of acknowledgements
        ++this->statistics.incomingAcknowledgementCounter;

        // notify listeners about the confirmed delivery
        this->handler.onDeliveryConfirmed(iterator->messageCounter, iterator->messageType);

        // do not proceed further
        return;
    }

    // get the message status of the peer from the payload
    const Definitions::MessageStatusType remoteMessageStatusType
        = frame.getPayloadLength() > 0 ? Definitions::toMessageStatusType(frame.getPayload()[0])
                                       : Definitions::MessageStatusType::UNKNOWN;

    // keep track of rejections and failed deliveries
    ++this->statistics.incomingRejectionCounter;
    ++this->statistics.failedDeliveryCounter;

    // notify listeners about the failed delivery
    this->handler.onDeliveryFailed(iterator->messageCounter,
                                   iterator->messageType,
                                   Definitions::DeliveryFailureType::REJECTED,
                                   remoteMessageStatusType);
}

const ReceivedMessage *Link::getReceivedMessageWithMessageCounter(const uint16_t messageCounter,
                                                                  const uint16_t messageType) const
{
    // received message must be found
    const std::array<ReceivedMessage, Settings::DefaultDuplicateHistorySize>::const_iterator &iterator
        = std::find_if(this->receivedMessages.cbegin(),
                       this->receivedMessages.cend(),
                       [messageCounter, messageType](const ReceivedMessage &receivedMessage) {
                           return receivedMessage.used
                                  && receivedMessage.messageCounter == messageCounter
                                  && receivedMessage.messageType == messageType;
                       });
    if (iterator == this->receivedMessages.cend()) {
        return nullptr;
    }

    // expose the received message
    return &*iterator;
}

void Link::rememberReceivedMessage(const uint16_t messageCounter,
                                   const uint16_t messageType,
                                   const Definitions::MessageStatusType messageStatusType)
{
    // refresh the entry of a counter that a restarted peer reuses
    const std::array<ReceivedMessage, Settings::DefaultDuplicateHistorySize>::iterator &iterator
        = std::find_if(this->receivedMessages.begin(),
                       this->receivedMessages.end(),
                       [messageCounter, messageType](const ReceivedMessage &receivedMessage) {
                           return receivedMessage.used
                                  && receivedMessage.messageCounter == messageCounter
                                  && receivedMessage.messageType == messageType;
                       });
    if (iterator != this->receivedMessages.end()) {
        iterator->messageStatusType = messageStatusType;
        return;
    }

    // overwrite the oldest entry
    ReceivedMessage &receivedMessage = this->receivedMessages[this->nextReceivedMessageIndex];
    receivedMessage.used = true;
    receivedMessage.messageCounter = messageCounter;
    receivedMessage.messageType = messageType;
    receivedMessage.messageStatusType = messageStatusType;

    // move to the next entry of the ring
    this->nextReceivedMessageIndex = (this->nextReceivedMessageIndex + 1)
                                     % this->receivedMessages.size();
}

} // namespace sercom::Link

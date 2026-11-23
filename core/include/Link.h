#ifndef SERCOM_LINK_H
#define SERCOM_LINK_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "../include/Clock.h"
#include "../include/Definitions.h"
#include "../include/Frame.h"
#include "../include/FrameDecoder.h"
#include "../include/Settings.h"
#include "../include/Transport.h"

namespace sercom::Link {

/*
 * Link configuration.
 */
struct LinkConfiguration
{
    std::chrono::milliseconds retransmitTimeout
        = Settings::DEFAULT_RETRANSMIT_TIMEOUT_MSEC; // wait for an answer in milliseconds
    uint8_t maximumRetransmissions
        = Settings::DEFAULT_MAXIMUM_RETRANSMISSIONS; // retransmissions before a delivery fails
    std::chrono::milliseconds interByteTimeout
        = Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC; // decoder silence limit in milliseconds
    bool routeCapable = false; // every sent frame carries the route capable flag
};

/*
 * Link statistics since construction.
 */
struct LinkStatistics
{
    uint64_t outgoingFrameCounter = 0;             // frames written to the transport
    uint64_t incomingFrameCounter = 0;             // valid frames from the decoder
    uint64_t deliveredMessageCounter = 0;          // messages handed to the application
    uint64_t outgoingAcknowledgementCounter = 0;   // sent ACK frames
    uint64_t incomingAcknowledgementCounter = 0;   // ACK frames of a pending message
    uint64_t outgoingRejectionCounter = 0;         // sent NACK frames
    uint64_t incomingRejectionCounter = 0;         // NACK frames of a pending message
    uint64_t retransmissionCounter = 0;            // frames sent again after a timeout
    uint64_t failedDeliveryCounter = 0;            // timed out or rejected messages
    uint64_t duplicateFrameCounter = 0;            // retransmissions of delivered messages
    uint64_t unexpectedAcknowledgementCounter = 0; // ACK or NACK frames without a pending message
    uint64_t transportErrorCounter = 0;            // frames the transport could not take
    FrameDecoder::DecoderStatistics decoderStatistics; // decoder statistics
};

/*
 * Acknowledged message that waits for an answer.
 */
struct PendingMessage
{
    bool used = false;                             // slot holds a message
    uint16_t messageCounter = 0;                   // message counter of the message
    uint16_t messageType = 0;                      // message type of the message
    uint8_t payloadLength = 0;                     // payload length in bytes
    uint8_t retransmissionCount = 0;               // retransmissions so far
    std::chrono::milliseconds lastSentTimestamp{}; // time of the latest transmission
    uint32_t sendSequence = 0;                     // acknowledged messages sent before it
    std::array<uint8_t, Settings::MaximumPayloadSize> payload{}; // payload for retransmissions
};

/*
 * Acknowledged message that was received recently.
 */
struct ReceivedMessage
{
    bool used = false;           // entry holds a message
    uint16_t messageCounter = 0; // message counter of the message
    uint16_t messageType = 0;    // message type of the message
    Definitions::MessageStatusType messageStatusType
        = Definitions::MessageStatusType::UNKNOWN; // message status sent back
};

class LinkHandler
{
public:
    /*
     * Receiver of link layer events, implemented by the application.
     */
    LinkHandler() = default;
    virtual ~LinkHandler();

    // delete copy constructor and copy assignment operator
    LinkHandler(const LinkHandler &) = delete;
    LinkHandler &operator=(const LinkHandler &) = delete;

    // delete move constructor and move assignment operator
    LinkHandler(LinkHandler &&) = delete;
    LinkHandler &operator=(LinkHandler &&) = delete;

public:
    /*
     * Listen for a received message and get the message status that answers a requested ACK.
     * The frame reference is valid during the call only.
     */
    [[nodiscard]] virtual Definitions::MessageStatusType onReceivedMessage(const Frame::Frame &frame)
        = 0;

    /*
     * Listen for an acknowledged message that reached the peer.
     * The slot of the message is free again during the call.
     */
    virtual void onDeliveryConfirmed(uint16_t messageCounter, uint16_t messageType) = 0;

    /*
     * Listen for an acknowledged message that timed out or was rejected by the peer.
     * The slot of the message is free again during the call.
     */
    virtual void onDeliveryFailed(uint16_t messageCounter,
                                  uint16_t messageType,
                                  Definitions::DeliveryFailureType deliveryFailureType,
                                  Definitions::MessageStatusType remoteMessageStatusType)
        = 0;
};

class Link final : public FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Link layer with message counters, acknowledgements, retransmissions and duplicate detection.
     */
    explicit Link(Transport::Transport &transport,
                  LinkHandler &handler,
                  const Clock::Clock &clock,
                  const LinkConfiguration &configuration = LinkConfiguration());
    ~Link() final;

    // delete copy constructor and copy assignment operator
    Link(const Link &) = delete;
    Link &operator=(const Link &) = delete;

    // delete move constructor and move assignment operator
    Link(Link &&) = delete;
    Link &operator=(Link &&) = delete;

public:
    /*
     * Send a message and store the message counter it carries.
     * Acknowledged messages are sent again until they are confirmed or fail.
     */
    [[nodiscard]] Definitions::SendStatusType sendMessage(uint16_t messageType,
                                                          const uint8_t *payload,
                                                          size_t payloadLength,
                                                          bool acknowledged,
                                                          uint16_t &messageCounter);

    /*
     * Listen for received data and hand it to the decoder.
     */
    void onReceivedData(const uint8_t *data, size_t size);

    /*
     * Maintain the retransmissions and the inter byte timeout.
     */
    void maintain();

public:
    /*
     * Get whether an acknowledged message can be sent now.
     */
    [[nodiscard]] bool getSendAcknowledgedAllowed() const;

    /*
     * Get the number of acknowledged messages that wait for an answer.
     */
    [[nodiscard]] size_t getPendingCount() const;

    /*
     * Get the message counter of the next message.
     */
    [[nodiscard]] uint16_t getNextMessageCounter() const;

    /*
     * Get the link statistics.
     */
    [[nodiscard]] LinkStatistics getStatistics() const;

private:
    /*
     * Listen for a received frame.
     */
    void onReceivedFrame(const Frame::Frame &frame) final;

    /*
     * Listen for a decode error.
     * Corrupted frames are not answered because the sender retransmits after its timeout.
     */
    void onDecodeError(Definitions::DecodeErrorType decodeErrorType) final;

private:
    /*
     * Get whether the oldest pending message still allows a new acknowledged message.
     */
    [[nodiscard]] bool getWithinSpread() const;

    /*
     * Get the flags of a sent frame together with the route capable flag of a route capable link.
     */
    [[nodiscard]] uint8_t getFrameFlags(uint8_t flags) const;

    /*
     * Send a frame to the transport.
     */
    [[nodiscard]] bool sendFrame(const Frame::Frame &frame);

    /*
     * Send the ACK or NACK frame that answers an acknowledged message.
     */
    void sendAcknowledgement(const Frame::Frame &frame,
                             Definitions::MessageStatusType messageStatusType);

    /*
     * Handle the ACK or NACK frame of a pending message.
     */
    void handleAcknowledgement(const Frame::Frame &frame);

    /*
     * Find a recently received acknowledged message with its message counter and message type.
     * It is null when no such message is remembered.
     */
    [[nodiscard]] const ReceivedMessage *getReceivedMessageWithMessageCounter(
        uint16_t messageCounter, uint16_t messageType) const;

    /*
     * Remember a received acknowledged message and the message status sent back.
     */
    void rememberReceivedMessage(uint16_t messageCounter,
                                 uint16_t messageType,
                                 Definitions::MessageStatusType messageStatusType);

private:
    Transport::Transport &transport;         // reference for transport
    LinkHandler &handler;                    // reference to handler
    const Clock::Clock &clock;               // reference for clock
    const LinkConfiguration configuration;   // link configuration
    FrameDecoder::FrameDecoder frameDecoder; // decoder of the received bytes
    size_t nextReceivedMessageIndex;         // history entry to overwrite next
    uint16_t nextMessageCounter;             // message counter of the next message
    uint32_t acknowledgedSendSequence;       // number of sent acknowledged messages
    LinkStatistics statistics;               // link statistics
    std::array<PendingMessage, Settings::MaximumPendingFrames> pendingMessages; // sent messages
    std::array<ReceivedMessage, Settings::DefaultDuplicateHistorySize>
        receivedMessages; // history of received messages
};

} // namespace sercom::Link

#endif // SERCOM_LINK_H

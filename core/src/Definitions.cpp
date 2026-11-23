#include "../include/Definitions.h"

namespace sercom::Definitions {

uint8_t fromFrameFlagType(const FrameFlagType frameFlagType)
{
    return static_cast<uint8_t>(frameFlagType);
}

FrameFlagType toFrameFlagType(const uint8_t frameFlagType)
{
    return static_cast<FrameFlagType>(frameFlagType);
}

std::string_view fromDecodeErrorType(const DecodeErrorType decodeErrorType)
{
    switch (decodeErrorType) {
    case DecodeErrorType::INVALID_VERSION: {
        return {"INVALID_VERSION"};
    }
    case DecodeErrorType::PAYLOAD_TOO_LONG: {
        return {"PAYLOAD_TOO_LONG"};
    }
    case DecodeErrorType::CRC_MISMATCH: {
        return {"CRC_MISMATCH"};
    }
    case DecodeErrorType::INTER_BYTE_TIMEOUT: {
        return {"INTER_BYTE_TIMEOUT"};
    }
    case DecodeErrorType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

DecodeErrorType toDecodeErrorType(const std::string_view decodeErrorType)
{
    if (decodeErrorType == std::string_view("INVALID_VERSION")) {
        return DecodeErrorType::INVALID_VERSION;
    }
    if (decodeErrorType == std::string_view("PAYLOAD_TOO_LONG")) {
        return DecodeErrorType::PAYLOAD_TOO_LONG;
    }
    if (decodeErrorType == std::string_view("CRC_MISMATCH")) {
        return DecodeErrorType::CRC_MISMATCH;
    }
    if (decodeErrorType == std::string_view("INTER_BYTE_TIMEOUT")) {
        return DecodeErrorType::INTER_BYTE_TIMEOUT;
    }
    return DecodeErrorType::UNKNOWN;
}

uint8_t fromMessageStatusType(const MessageStatusType messageStatusType)
{
    return static_cast<uint8_t>(messageStatusType);
}

MessageStatusType toMessageStatusType(const uint8_t messageStatusType)
{
    switch (static_cast<MessageStatusType>(messageStatusType)) {
    case MessageStatusType::ACCEPTED: {
        return MessageStatusType::ACCEPTED;
    }
    case MessageStatusType::UNSUPPORTED_MESSAGE_TYPE: {
        return MessageStatusType::UNSUPPORTED_MESSAGE_TYPE;
    }
    case MessageStatusType::MALFORMED_PAYLOAD: {
        return MessageStatusType::MALFORMED_PAYLOAD;
    }
    case MessageStatusType::BUSY: {
        return MessageStatusType::BUSY;
    }
    case MessageStatusType::INVALID_ARGUMENT: {
        return MessageStatusType::INVALID_ARGUMENT;
    }
    case MessageStatusType::UNKNOWN:
    default: {
        return MessageStatusType::UNKNOWN;
    }
    }
}

std::string_view fromSendStatusType(const SendStatusType sendStatusType)
{
    switch (sendStatusType) {
    case SendStatusType::SENT: {
        return {"SENT"};
    }
    case SendStatusType::WINDOW_FULL: {
        return {"WINDOW_FULL"};
    }
    case SendStatusType::PAYLOAD_TOO_LONG: {
        return {"PAYLOAD_TOO_LONG"};
    }
    case SendStatusType::TRANSPORT_ERROR: {
        return {"TRANSPORT_ERROR"};
    }
    case SendStatusType::INVALID_ARGUMENT: {
        return {"INVALID_ARGUMENT"};
    }
    case SendStatusType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

SendStatusType toSendStatusType(const std::string_view sendStatusType)
{
    if (sendStatusType == std::string_view("SENT")) {
        return SendStatusType::SENT;
    }
    if (sendStatusType == std::string_view("WINDOW_FULL")) {
        return SendStatusType::WINDOW_FULL;
    }
    if (sendStatusType == std::string_view("PAYLOAD_TOO_LONG")) {
        return SendStatusType::PAYLOAD_TOO_LONG;
    }
    if (sendStatusType == std::string_view("TRANSPORT_ERROR")) {
        return SendStatusType::TRANSPORT_ERROR;
    }
    if (sendStatusType == std::string_view("INVALID_ARGUMENT")) {
        return SendStatusType::INVALID_ARGUMENT;
    }
    return SendStatusType::UNKNOWN;
}

std::string_view fromDeliveryFailureType(const DeliveryFailureType deliveryFailureType)
{
    switch (deliveryFailureType) {
    case DeliveryFailureType::TIMED_OUT: {
        return {"TIMED_OUT"};
    }
    case DeliveryFailureType::REJECTED: {
        return {"REJECTED"};
    }
    case DeliveryFailureType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

DeliveryFailureType toDeliveryFailureType(const std::string_view deliveryFailureType)
{
    if (deliveryFailureType == std::string_view("TIMED_OUT")) {
        return DeliveryFailureType::TIMED_OUT;
    }
    if (deliveryFailureType == std::string_view("REJECTED")) {
        return DeliveryFailureType::REJECTED;
    }
    return DeliveryFailureType::UNKNOWN;
}

std::string_view fromPeerVersionType(const PeerVersionType peerVersionType)
{
    switch (peerVersionType) {
    case PeerVersionType::VERSION_1: {
        return {"VERSION_1"};
    }
    case PeerVersionType::VERSION_2: {
        return {"VERSION_2"};
    }
    case PeerVersionType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

PeerVersionType toPeerVersionType(const std::string_view peerVersionType)
{
    if (peerVersionType == std::string_view("VERSION_1")) {
        return PeerVersionType::VERSION_1;
    }
    if (peerVersionType == std::string_view("VERSION_2")) {
        return PeerVersionType::VERSION_2;
    }
    return PeerVersionType::UNKNOWN;
}

bool isFrameFlagSet(const uint8_t flags, const FrameFlagType frameFlagType)
{
    // expose whether the bit of the flag is set
    return (flags & fromFrameFlagType(frameFlagType)) != 0;
}

std::string_view getFrameFlagTypeText(const FrameFlagType frameFlagType)
{
    switch (frameFlagType) {
    case FrameFlagType::ACK_REQUEST: {
        return {"ACK_REQUEST"};
    }
    case FrameFlagType::ACK: {
        return {"ACK"};
    }
    case FrameFlagType::NACK: {
        return {"NACK"};
    }
    case FrameFlagType::RETRANSMISSION: {
        return {"RETRANSMISSION"};
    }
    case FrameFlagType::ROUTE_CAPABLE: {
        return {"ROUTE_CAPABLE"};
    }
    default: {
        return {"UNKNOWN"};
    }
    }
}

size_t getHeaderSize(const uint8_t version)
{
    // expose the header size of a direct frame
    if (version == PROTOCOL_VERSION) {
        return HEADER_SIZE;
    }

    // expose the header size of a routed frame
    if (version == ROUTED_PROTOCOL_VERSION) {
        return ROUTED_HEADER_SIZE;
    }

    // expose no header size for an unknown version
    return 0;
}

} // namespace sercom::Definitions

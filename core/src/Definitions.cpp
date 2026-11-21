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
    default: {
        return {"UNKNOWN"};
    }
    }
}

} // namespace sercom::Definitions

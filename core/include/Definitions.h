#ifndef SERCOM_DEFINITIONS_H
#define SERCOM_DEFINITIONS_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace sercom::Definitions {

constexpr uint8_t SYNC_BYTE_1 = 0xA5;   // first byte of the sync word
constexpr uint8_t SYNC_BYTE_2 = 0xC3;   // second byte of the sync word
constexpr uint8_t PROTOCOL_VERSION = 1; // version of the frame format

constexpr size_t SYNC_SIZE = 2;            // sync word size in bytes
constexpr size_t VERSION_SIZE = 1;         // version field size in bytes
constexpr size_t FLAGS_SIZE = 1;           // flags field size in bytes
constexpr size_t MESSAGE_COUNTER_SIZE = 2; // message counter field size in bytes
constexpr size_t MESSAGE_TYPE_SIZE = 2;    // message type field size in bytes
constexpr size_t PAYLOAD_LENGTH_SIZE = 1;  // payload length field size in bytes
constexpr size_t CRC_SIZE = 4;             // CRC32 field size in bytes
constexpr size_t HEADER_SIZE = SYNC_SIZE + VERSION_SIZE + FLAGS_SIZE + MESSAGE_COUNTER_SIZE
                               + MESSAGE_TYPE_SIZE + PAYLOAD_LENGTH_SIZE; // header size in bytes
constexpr size_t FRAME_OVERHEAD = HEADER_SIZE + CRC_SIZE; // frame size without payload in bytes

constexpr uint32_t CRC32_POLYNOMIAL = 0xEDB88320U;      // reflected polynomial of CRC-32/ISO-HDLC
constexpr uint32_t CRC32_INITIAL_VALUE = 0xFFFFFFFFU;   // CRC32 register before the first byte
constexpr uint32_t CRC32_FINAL_XOR_VALUE = 0xFFFFFFFFU; // value XORed into the CRC32 register

/*
 * Frame flag type.
 */
enum class FrameFlagType : uint8_t {
    ACK_REQUEST = 1 << 0,    // receiver must answer the frame
    ACK = 1 << 1,            // frame confirms a message
    NACK = 1 << 2,           // frame rejects a message
    RETRANSMISSION = 1 << 3, // frame repeats an earlier transmission
};

/*
 * Frame flag type to integer.
 */
uint8_t fromFrameFlagType(FrameFlagType frameFlagType);

/*
 * Integer to frame flag type.
 */
FrameFlagType toFrameFlagType(uint8_t frameFlagType);

/*
 * Decode error type.
 */
enum class DecodeErrorType : uint8_t {
    UNKNOWN = 0,            // unknown decode error
    INVALID_VERSION = 1,    // version field is not the protocol version
    PAYLOAD_TOO_LONG = 2,   // payload length is above the limit of this build
    CRC_MISMATCH = 3,       // CRC32 does not match the frame
    INTER_BYTE_TIMEOUT = 4, // line was silent inside a frame
};

/*
 * Decode error type to string.
 */
std::string_view fromDecodeErrorType(DecodeErrorType decodeErrorType);

/*
 * String to decode error type.
 */
DecodeErrorType toDecodeErrorType(std::string_view decodeErrorType);

/*
 * Message status type.
 */
enum class MessageStatusType : uint8_t {
    UNKNOWN = 0,                  // reason not given or not understood
    ACCEPTED = 1,                 // message is accepted
    UNSUPPORTED_MESSAGE_TYPE = 2, // receiver does not implement the message type
    MALFORMED_PAYLOAD = 3,        // payload does not match the message type
    BUSY = 4,                     // receiver cannot handle the message now
    INVALID_ARGUMENT = 5,         // field of the payload is out of range
};

/*
 * Message status type to integer.
 */
uint8_t fromMessageStatusType(MessageStatusType messageStatusType);

/*
 * Integer to message status type.
 */
MessageStatusType toMessageStatusType(uint8_t messageStatusType);

/*
 * Send status type.
 */
enum class SendStatusType : uint8_t {
    UNKNOWN = 0,          // unknown send status
    SENT = 1,             // frame is written to the transport
    WINDOW_FULL = 2,      // no free slot for an acknowledged message
    PAYLOAD_TOO_LONG = 3, // payload is above the limit of this build
    TRANSPORT_ERROR = 4,  // transport could not write the frame
    INVALID_ARGUMENT = 5, // argument is missing or does not fit into its message field
};

/*
 * Send status type to string.
 */
std::string_view fromSendStatusType(SendStatusType sendStatusType);

/*
 * String to send status type.
 */
SendStatusType toSendStatusType(std::string_view sendStatusType);

/*
 * Delivery failure type.
 */
enum class DeliveryFailureType : uint8_t {
    UNKNOWN = 0,   // unknown delivery failure
    TIMED_OUT = 1, // every retransmission stayed unanswered
    REJECTED = 2,  // peer answered with a NACK
};

/*
 * Delivery failure type to string.
 */
std::string_view fromDeliveryFailureType(DeliveryFailureType deliveryFailureType);

/*
 * String to delivery failure type.
 */
DeliveryFailureType toDeliveryFailureType(std::string_view deliveryFailureType);

/*
 * Check whether a flag is set in the value of a FLAGS field.
 */
[[nodiscard]] bool isFrameFlagSet(uint8_t flags, FrameFlagType frameFlagType);

/*
 * Get frame flag type as text.
 */
[[nodiscard]] std::string_view getFrameFlagTypeText(FrameFlagType frameFlagType);

} // namespace sercom::Definitions

// check definitions
static_assert(sercom::Definitions::HEADER_SIZE == 9, "Header size must be 9 bytes");
static_assert(sercom::Definitions::FRAME_OVERHEAD == 13, "Frame overhead must be 13 bytes");

#endif // SERCOM_DEFINITIONS_H

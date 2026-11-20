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
 * Check whether a flag is set in the value of a FLAGS field.
 */
[[nodiscard]] bool isFrameFlagSet(uint8_t flags, FrameFlagType frameFlagType);

/*
 * Get frame flag type as text.
 */
[[nodiscard]] std::string_view getFrameFlagTypeText(FrameFlagType frameFlagType);

} // namespace sercom::Definitions

#endif // SERCOM_DEFINITIONS_H

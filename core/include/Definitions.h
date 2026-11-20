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

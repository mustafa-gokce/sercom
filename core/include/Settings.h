#ifndef SERCOM_SETTINGS_H
#define SERCOM_SETTINGS_H

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "../include/Definitions.h"

namespace sercom::Settings {

using namespace std::chrono_literals;

constexpr size_t MaximumPayloadSize = MAXIMUM_PAYLOAD_SIZE;     // maximum payload size in bytes
constexpr size_t MaximumPendingFrames = MAXIMUM_PENDING_FRAMES; // maximum unacknowledged frames
constexpr size_t DefaultDuplicateHistorySize
    = DEFAULT_DUPLICATE_HISTORY_SIZE; // default number of remembered acknowledged messages

constexpr size_t MAXIMUM_DIRECT_FRAME_SIZE
    = Definitions::FRAME_OVERHEAD + MaximumPayloadSize; // maximum direct frame size in bytes
constexpr size_t MAXIMUM_FRAME_SIZE = Definitions::ROUTED_FRAME_OVERHEAD
                                      + MaximumPayloadSize; // maximum frame size in bytes
constexpr size_t MAXIMUM_ACKNOWLEDGED_SPREAD = DefaultDuplicateHistorySize - MaximumPendingFrames
                                               + 1; // maximum acknowledged messages after the oldest

constexpr std::chrono::milliseconds DEFAULT_INTER_BYTE_TIMEOUT_MSEC
    = 50ms; // default silence that drops a partial frame in milliseconds
constexpr std::chrono::milliseconds DEFAULT_RETRANSMIT_TIMEOUT_MSEC
    = 250ms; // default wait for an answer in milliseconds

constexpr uint8_t DEFAULT_MAXIMUM_RETRANSMISSIONS = 5; // default retransmissions of a message
constexpr uint8_t DEFAULT_TIME_TO_LIVE = 8;            // default links a routed frame may cross

constexpr size_t MAXIMUM_UNFRAMED_DATA_SIZE = 80;  // maximum unframed data size in bytes
constexpr size_t MAXIMUM_TRACED_PAYLOAD_SIZE = 16; // maximum traced payload size in bytes
constexpr size_t TRACED_MESSAGE_COUNTER_WIDTH = 5; // message counter column width in characters
constexpr size_t TRACED_MESSAGE_TYPE_WIDTH = 28;   // message type column width in characters
constexpr size_t TRACED_FLAGS_WIDTH = 26;          // flags column width in characters
constexpr size_t TRACED_PAYLOAD_LENGTH_WIDTH = 3;  // payload length column width in characters

} // namespace sercom::Settings

// check settings
static_assert(sercom::Settings::MaximumPayloadSize <= 0xFF,
              "Maximum payload size must be at most 255 bytes");
static_assert(sercom::Settings::MaximumPayloadSize >= 16,
              "Maximum payload size must be at least 16 bytes");
static_assert(sercom::Settings::MaximumPendingFrames >= 1,
              "Maximum pending frames must be at least 1");
static_assert(sercom::Settings::DefaultDuplicateHistorySize
                  >= sercom::Settings::MaximumPendingFrames,
              "Default duplicate history size must be at least the maximum pending frames");

#endif // SERCOM_SETTINGS_H

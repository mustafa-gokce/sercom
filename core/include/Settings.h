#ifndef SERCOM_SETTINGS_H
#define SERCOM_SETTINGS_H

#include <cstddef>

#include "../include/Definitions.h"

namespace sercom::Settings {

constexpr size_t MaximumPayloadSize = MAXIMUM_PAYLOAD_SIZE; // maximum payload size in bytes

constexpr size_t MAXIMUM_FRAME_SIZE = Definitions::FRAME_OVERHEAD
                                      + MaximumPayloadSize; // maximum frame size in bytes

} // namespace sercom::Settings

// check settings
static_assert(sercom::Settings::MaximumPayloadSize <= 0xFF,
              "Maximum payload size must be at most 255 bytes");
static_assert(sercom::Settings::MaximumPayloadSize >= 16,
              "Maximum payload size must be at least 16 bytes");

#endif // SERCOM_SETTINGS_H

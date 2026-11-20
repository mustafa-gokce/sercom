#ifndef SERCOM_DEFINITIONS_H
#define SERCOM_DEFINITIONS_H

#include <cstdint>

namespace sercom::Definitions {

constexpr uint8_t SYNC_BYTE_1 = 0xA5; // first byte of the sync word
constexpr uint8_t SYNC_BYTE_2 = 0xC3; // second byte of the sync word

} // namespace sercom::Definitions

#endif // SERCOM_DEFINITIONS_H

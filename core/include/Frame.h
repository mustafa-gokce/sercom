#ifndef SERCOM_FRAME_H
#define SERCOM_FRAME_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "../include/Definitions.h"
#include "../include/Settings.h"

namespace sercom::Frame {

class Frame final
{
public:
    /*
     * Frame of the wire format that keeps its bytes from the sync word to the CRC32.
     * A frame built from a payload above the limit stays empty.
     */
    explicit Frame(uint8_t flags,
                   uint16_t messageCounter,
                   uint16_t messageType,
                   const uint8_t *payload,
                   size_t payloadLength);
    ~Frame() = default;

    // default copy constructor and copy assignment operator
    Frame(const Frame &) = default;
    Frame &operator=(const Frame &) = default;

    // default move constructor and move assignment operator
    Frame(Frame &&) = default;
    Frame &operator=(Frame &&) = default;

public:
    /*
     * Get the first frame byte.
     */
    [[nodiscard]] const uint8_t *getData() const;

    /*
     * Get the frame size in bytes.
     */
    [[nodiscard]] size_t getSize() const;

private:
    std::array<uint8_t, Settings::MAXIMUM_FRAME_SIZE> frame; // frame as byte array
    size_t frameSize;                                        // frame size in bytes
};

} // namespace sercom::Frame

#endif // SERCOM_FRAME_H

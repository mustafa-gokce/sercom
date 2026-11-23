#ifndef SERCOM_FRAME_H
#define SERCOM_FRAME_H

#include <array>
#include <cstddef>
#include <cstdint>

#include "../include/Definitions.h"
#include "../include/Settings.h"

namespace sercom::Frame {

/*
 * Route header of a routed frame.
 */
struct RouteHeader
{
    uint8_t sourceAddress = Definitions::UNASSIGNED_ADDRESS;      // node that started the frame
    uint8_t destinationAddress = Definitions::UNASSIGNED_ADDRESS; // node the frame is for
    uint8_t timeToLive = 0;                                       // links the frame may still cross
};

class Frame final
{
public:
    /*
     * Frame of the wire format that keeps its bytes from the sync word to the CRC32.
     * A frame built with a route header is a routed frame, without one it is a direct frame.
     * A frame built from a payload above the limit or from too many bytes stays empty.
     */
    explicit Frame(uint8_t flags,
                   uint16_t messageCounter,
                   uint16_t messageType,
                   const uint8_t *payload,
                   size_t payloadLength);
    explicit Frame(uint8_t flags,
                   uint16_t messageCounter,
                   uint16_t messageType,
                   const RouteHeader &routeHeader,
                   const uint8_t *payload,
                   size_t payloadLength);
    explicit Frame(const uint8_t *data, size_t size);
    ~Frame() = default;

    // default copy constructor and copy assignment operator
    Frame(const Frame &) = default;
    Frame &operator=(const Frame &) = default;

    // default move constructor and move assignment operator
    Frame(Frame &&) = default;
    Frame &operator=(Frame &&) = default;

public:
    /*
     * Get the version.
     */
    [[nodiscard]] uint8_t getVersion() const;

    /*
     * Get the flags.
     */
    [[nodiscard]] uint8_t getFlags() const;

    /*
     * Get the message counter.
     */
    [[nodiscard]] uint16_t getMessageCounter() const;

    /*
     * Get the message type.
     */
    [[nodiscard]] uint16_t getMessageType() const;

    /*
     * Get the payload length in bytes.
     */
    [[nodiscard]] uint8_t getPayloadLength() const;

    /*
     * Get the source address, unassigned for a direct frame.
     */
    [[nodiscard]] uint8_t getSourceAddress() const;

    /*
     * Get the destination address, unassigned for a direct frame.
     */
    [[nodiscard]] uint8_t getDestinationAddress() const;

    /*
     * Get the time to live, zero for a direct frame.
     */
    [[nodiscard]] uint8_t getTimeToLive() const;

    /*
     * Get the first payload byte.
     */
    [[nodiscard]] const uint8_t *getPayload() const;

    /*
     * Get the CRC32.
     */
    [[nodiscard]] uint32_t getCRC32() const;

public:
    /*
     * Get whether a flag is set.
     */
    [[nodiscard]] bool getFlag(Definitions::FrameFlagType frameFlagType) const;

    /*
     * Get whether the frame is a routed frame that holds its route header.
     */
    [[nodiscard]] bool getRouted() const;

    /*
     * Get the route header, empty for a direct frame.
     */
    [[nodiscard]] RouteHeader getRouteHeader() const;

    /*
     * Get the header size in bytes, the size of the direct header unless the frame is routed.
     */
    [[nodiscard]] size_t getHeaderSize() const;

    /*
     * Get whether the sync word, the version and the payload length fit the frame size.
     */
    [[nodiscard]] bool getLikelyValid() const;

    /*
     * Get whether the frame is likely valid and its CRC32 matches.
     */
    [[nodiscard]] bool getCRCValid() const;

    /*
     * Get the first frame byte.
     */
    [[nodiscard]] const uint8_t *getData() const;

    /*
     * Get the frame size in bytes.
     */
    [[nodiscard]] size_t getSize() const;

private:
    /*
     * Write a frame of the version with its fields, the route header only into a routed frame.
     */
    void writeFrame(uint8_t version,
                    uint8_t flags,
                    uint16_t messageCounter,
                    uint16_t messageType,
                    const RouteHeader &routeHeader,
                    const uint8_t *payload,
                    size_t payloadLength);

private:
    std::array<uint8_t, Settings::MAXIMUM_FRAME_SIZE> frame; // frame as byte array
    size_t frameSize;                                        // frame size in bytes
};

} // namespace sercom::Frame

#endif // SERCOM_FRAME_H

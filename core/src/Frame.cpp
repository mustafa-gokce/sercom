#include "../include/Frame.h"

#include <algorithm>

#include "../include/ByteCodec.h"
#include "../include/CRC32.h"

namespace sercom::Frame {

Frame::Frame(const uint8_t flags,
             const uint16_t messageCounter,
             const uint16_t messageType,
             const uint8_t *payload,
             const size_t payloadLength)
    : frame()
    , frameSize(0)
{
    // write the direct frame
    this->writeFrame(Definitions::PROTOCOL_VERSION,
                     flags,
                     messageCounter,
                     messageType,
                     RouteHeader(),
                     payload,
                     payloadLength);
}

Frame::Frame(const uint8_t flags,
             const uint16_t messageCounter,
             const uint16_t messageType,
             const RouteHeader &routeHeader,
             const uint8_t *payload,
             const size_t payloadLength)
    : frame()
    , frameSize(0)
{
    // write the routed frame
    this->writeFrame(Definitions::ROUTED_PROTOCOL_VERSION,
                     flags,
                     messageCounter,
                     messageType,
                     routeHeader,
                     payload,
                     payloadLength);
}

Frame::Frame(const uint8_t *data, const size_t size)
    : frame()
    , frameSize(0)
{
    // data must exist
    if (data == nullptr) {
        return;
    }

    // data must fit into a frame of this build
    if (size > this->frame.size()) {
        return;
    }

    // copy the received bytes
    std::copy_n(data, size, this->frame.begin());
    this->frameSize = size;
}

uint8_t Frame::getVersion() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return 0;
    }

    // expose the version
    return this->frame[2];
}

uint8_t Frame::getFlags() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return 0;
    }

    // expose the flags
    return this->frame[3];
}

uint16_t Frame::getMessageCounter() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return 0;
    }

    // expose the message counter
    return ByteCodec::loadUint16(this->frame.data() + 4);
}

uint16_t Frame::getMessageType() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return 0;
    }

    // expose the message type
    return ByteCodec::loadUint16(this->frame.data() + 6);
}

uint8_t Frame::getPayloadLength() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return 0;
    }

    // expose the payload length
    return this->frame[8];
}

uint8_t Frame::getSourceAddress() const
{
    // frame must hold a route header
    if (!this->getRouted()) {
        return Definitions::UNASSIGNED_ADDRESS;
    }

    // expose the source address
    return this->frame[9];
}

uint8_t Frame::getDestinationAddress() const
{
    // frame must hold a route header
    if (!this->getRouted()) {
        return Definitions::UNASSIGNED_ADDRESS;
    }

    // expose the destination address
    return this->frame[10];
}

uint8_t Frame::getTimeToLive() const
{
    // frame must hold a route header
    if (!this->getRouted()) {
        return 0;
    }

    // expose the time to live
    return this->frame[11];
}

const uint8_t *Frame::getPayload() const
{
    // expose the payload behind the header
    return this->frame.data() + this->getHeaderSize();
}

uint32_t Frame::getCRC32() const
{
    // frame must hold the CRC32 behind the payload
    const size_t crcOffset = this->getHeaderSize() + this->getPayloadLength();
    if (this->frameSize < crcOffset + Definitions::CRC_SIZE) {
        return 0;
    }

    // expose the CRC32
    return ByteCodec::loadUint32(this->frame.data() + crcOffset);
}

bool Frame::getFlag(const Definitions::FrameFlagType frameFlagType) const
{
    // expose whether the flag is set
    return Definitions::isFrameFlagSet(this->getFlags(), frameFlagType);
}

bool Frame::getRouted() const
{
    // expose whether a routed frame holds its route header
    return this->getVersion() == Definitions::ROUTED_PROTOCOL_VERSION
           && this->frameSize >= Definitions::ROUTED_HEADER_SIZE;
}

RouteHeader Frame::getRouteHeader() const
{
    // create route header from its fields
    RouteHeader routeHeader;
    routeHeader.sourceAddress = this->getSourceAddress();
    routeHeader.destinationAddress = this->getDestinationAddress();
    routeHeader.timeToLive = this->getTimeToLive();

    // expose the route header
    return routeHeader;
}

size_t Frame::getHeaderSize() const
{
    // expose the header size of a routed frame
    if (this->getRouted()) {
        return Definitions::ROUTED_HEADER_SIZE;
    }

    // expose the header size of every other frame
    return Definitions::HEADER_SIZE;
}

bool Frame::getLikelyValid() const
{
    // frame must hold a header
    if (this->frameSize < Definitions::HEADER_SIZE) {
        return false;
    }

    // frame must start with the sync word
    if (this->frame[0] != Definitions::SYNC_BYTE_1 || this->frame[1] != Definitions::SYNC_BYTE_2) {
        return false;
    }

    // version must be a protocol version with its header size
    const size_t currentHeaderSize = Definitions::getHeaderSize(this->getVersion());
    if (currentHeaderSize == 0) {
        return false;
    }

    // payload must fit into a frame of this build
    const size_t currentPayloadLength = this->getPayloadLength();
    if (currentPayloadLength > Settings::MaximumPayloadSize) {
        return false;
    }

    // frame size must follow from the header size and the payload length
    if (this->frameSize != currentHeaderSize + Definitions::CRC_SIZE + currentPayloadLength) {
        return false;
    }

    // passed basic checks
    return true;
}

bool Frame::getCRCValid() const
{
    // frame must be likely valid
    if (!this->getLikelyValid()) {
        return false;
    }

    // calculate the CRC32 of everything between the sync word and the CRC32
    const uint32_t crc = CRC32::calculateCRC32(this->frame.data() + Definitions::SYNC_SIZE,
                                               this->frameSize - Definitions::SYNC_SIZE
                                                   - Definitions::CRC_SIZE);

    // expose whether the CRC32 matches
    return crc == this->getCRC32();
}

const uint8_t *Frame::getData() const
{
    // expose the frame bytes
    return this->frame.data();
}

size_t Frame::getSize() const
{
    // expose the frame size
    return this->frameSize;
}

void Frame::writeFrame(const uint8_t version,
                       const uint8_t flags,
                       const uint16_t messageCounter,
                       const uint16_t messageType,
                       const RouteHeader &routeHeader,
                       const uint8_t *payload,
                       const size_t payloadLength)
{
    // payload must fit into a frame of this build
    if (payloadLength > Settings::MaximumPayloadSize) {
        return;
    }

    // payload must exist when it is not empty
    if (payloadLength > 0 && payload == nullptr) {
        return;
    }

    // write the header
    ByteCodec::ByteWriter byteWriter(this->frame.data(), this->frame.size());
    byteWriter.writeUint8(Definitions::SYNC_BYTE_1);
    byteWriter.writeUint8(Definitions::SYNC_BYTE_2);
    byteWriter.writeUint8(version);
    byteWriter.writeUint8(flags);
    byteWriter.writeUint16(messageCounter);
    byteWriter.writeUint16(messageType);
    byteWriter.writeUint8(static_cast<uint8_t>(payloadLength));

    // write the route header of a routed frame
    if (version == Definitions::ROUTED_PROTOCOL_VERSION) {
        byteWriter.writeUint8(routeHeader.sourceAddress);
        byteWriter.writeUint8(routeHeader.destinationAddress);
        byteWriter.writeUint8(routeHeader.timeToLive);
    }

    // write the payload
    byteWriter.writeBytes(payload, payloadLength);

    // write the CRC32 of everything after the sync word
    const uint32_t crc = CRC32::calculateCRC32(this->frame.data() + Definitions::SYNC_SIZE,
                                               byteWriter.getSize() - Definitions::SYNC_SIZE);
    byteWriter.writeUint32(crc);

    // keep the frame size
    this->frameSize = byteWriter.getSize();
}

} // namespace sercom::Frame

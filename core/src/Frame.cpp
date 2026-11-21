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
    // payload must fit into a frame of this build
    if (payloadLength > Settings::MaximumPayloadSize) {
        return;
    }

    // payload must exist when it is not empty
    if (payloadLength > 0 && payload == nullptr) {
        return;
    }

    // write the header and the payload
    ByteCodec::ByteWriter byteWriter(this->frame.data(), this->frame.size());
    byteWriter.writeUint8(Definitions::SYNC_BYTE_1);
    byteWriter.writeUint8(Definitions::SYNC_BYTE_2);
    byteWriter.writeUint8(Definitions::PROTOCOL_VERSION);
    byteWriter.writeUint8(flags);
    byteWriter.writeUint16(messageCounter);
    byteWriter.writeUint16(messageType);
    byteWriter.writeUint8(static_cast<uint8_t>(payloadLength));
    byteWriter.writeBytes(payload, payloadLength);

    // write the CRC32 of everything after the sync word
    const uint32_t crc = CRC32::calculateCRC32(this->frame.data() + Definitions::SYNC_SIZE,
                                               byteWriter.getSize() - Definitions::SYNC_SIZE);
    byteWriter.writeUint32(crc);

    // keep the frame size
    this->frameSize = byteWriter.getSize();
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

const uint8_t *Frame::getPayload() const
{
    // expose the payload behind the header
    return this->frame.data() + Definitions::HEADER_SIZE;
}

uint32_t Frame::getCRC32() const
{
    // frame must hold the CRC32 behind the payload
    const size_t crcOffset = Definitions::HEADER_SIZE + this->getPayloadLength();
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

    // version must be the protocol version
    if (this->getVersion() != Definitions::PROTOCOL_VERSION) {
        return false;
    }

    // payload must fit into a frame of this build
    const size_t currentPayloadLength = this->getPayloadLength();
    if (currentPayloadLength > Settings::MaximumPayloadSize) {
        return false;
    }

    // frame size must follow from the payload length
    if (this->frameSize != Definitions::FRAME_OVERHEAD + currentPayloadLength) {
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

} // namespace sercom::Frame

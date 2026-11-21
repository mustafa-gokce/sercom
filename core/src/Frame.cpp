#include "../include/Frame.h"

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

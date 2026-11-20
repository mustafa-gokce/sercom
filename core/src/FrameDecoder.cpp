#include "../include/FrameDecoder.h"

#include <algorithm>
#include <cstring>

namespace sercom::FrameDecoder {

FrameDecoderHandler::~FrameDecoderHandler() {}

FrameDecoder::FrameDecoder(FrameDecoderHandler &handler)
    : handler(handler)
    , buffer()
    , bufferStart(0)
    , bufferEnd(0)
    , lastByteTimestamp(0)
    , statistics()
{}

FrameDecoder::~FrameDecoder() {}

void FrameDecoder::onReceivedData(const uint8_t *data,
                                  const size_t size,
                                  const std::chrono::milliseconds currentTime)
{
    // data must exist
    if (data == nullptr) {
        return;
    }

    // iterate over the received bytes
    std::for_each(data, data + size, [this, currentTime](const uint8_t byte) {
        this->onReceivedByte(byte, currentTime);
    });
}

void FrameDecoder::onReceivedByte(const uint8_t byte, const std::chrono::milliseconds currentTime)
{
    // keep track of bytes
    ++this->statistics.incomingByteCounter;
    this->lastByteTimestamp = currentTime;

    // buffer the byte
    this->appendByte(byte);

    // process the buffered bytes
    this->processBuffer();
}

DecoderStatistics FrameDecoder::getStatistics() const
{
    // expose the decoder statistics
    return this->statistics;
}

size_t FrameDecoder::getBufferedSize() const
{
    // expose the number of buffered bytes
    return this->bufferEnd - this->bufferStart;
}

void FrameDecoder::processBuffer()
{
    // walk the buffered bytes one frame candidate at a time
    while (this->getBufferedSize() > 0) {
        // frame candidate must start with the first sync byte
        if (this->getBufferedByte(0) != Definitions::SYNC_BYTE_1) {
            this->discardBytes(1, true);
            continue;
        }

        // second sync byte must be buffered
        if (this->getBufferedSize() < Definitions::SYNC_SIZE) {
            break;
        }

        // second byte must be the second sync byte
        if (this->getBufferedByte(1) != Definitions::SYNC_BYTE_2) {
            this->discardBytes(1, true);
            continue;
        }

        // whole header must be buffered
        if (this->getBufferedSize() < Definitions::HEADER_SIZE) {
            break;
        }

        // get the payload length from the header
        const uint8_t *candidate = this->buffer.data() + this->bufferStart;
        const Frame::Frame frameHeader(candidate, Definitions::HEADER_SIZE);
        const size_t payloadLength = frameHeader.getPayloadLength();

        // payload and CRC32 must be buffered
        const size_t frameSize = Definitions::FRAME_OVERHEAD + payloadLength;
        if (this->getBufferedSize() < frameSize) {
            break;
        }

        // copy the frame out of the buffer
        const Frame::Frame frame(candidate, frameSize);

        // consume the frame
        this->discardBytes(frameSize, false);

        // keep track of frames
        ++this->statistics.incomingFrameCounter;

        // notify listeners about the received frame
        this->handler.onReceivedFrame(frame);
    }
}

void FrameDecoder::discardBytes(const size_t byteCount, const bool countAsDiscarded)
{
    // discard at most the buffered bytes
    const size_t discardedByteCount = std::min(byteCount, this->getBufferedSize());
    this->bufferStart += discardedByteCount;

    // keep track of bytes that never became part of a frame
    if (countAsDiscarded) {
        this->statistics.discardedByteCounter += discardedByteCount;
    }

    // start again at the front of an empty buffer
    if (this->bufferStart == this->bufferEnd) {
        this->bufferStart = 0;
        this->bufferEnd = 0;
    }
}

void FrameDecoder::appendByte(const uint8_t byte)
{
    // drop the oldest byte of a full buffer
    if (this->getBufferedSize() == this->buffer.size()) {
        this->discardBytes(1, true);
    }

    // move the buffered bytes to the front when the end is reached
    if (this->bufferEnd == this->buffer.size()) {
        const size_t currentBufferedSize = this->getBufferedSize();
        std::memmove(this->buffer.data(),
                     this->buffer.data() + this->bufferStart,
                     currentBufferedSize);
        this->bufferStart = 0;
        this->bufferEnd = currentBufferedSize;
    }

    // store the byte
    this->buffer[this->bufferEnd] = byte;
    ++this->bufferEnd;
}

uint8_t FrameDecoder::getBufferedByte(const size_t index) const
{
    // byte must be buffered
    if (index >= this->getBufferedSize()) {
        return 0;
    }

    // expose the buffered byte
    return this->buffer[this->bufferStart + index];
}

} // namespace sercom::FrameDecoder

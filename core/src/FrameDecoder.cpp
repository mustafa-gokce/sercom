#include "../include/FrameDecoder.h"

#include <algorithm>
#include <cstring>

namespace sercom::FrameDecoder {

using namespace std::chrono_literals;

FrameDecoderHandler::~FrameDecoderHandler() {}

FrameDecoder::FrameDecoder(FrameDecoderHandler &handler,
                           const std::chrono::milliseconds interByteTimeout)
    : handler(handler)
    , interByteTimeout(interByteTimeout)
    , buffer()
    , bufferStart(0)
    , bufferEnd(0)
    , lastByteTimestamp(0)
    , statistics()
    , processing(false)
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
    // drop a partial frame of an ended transmission
    this->checkTimeout(currentTime);

    // keep track of bytes
    ++this->statistics.incomingByteCounter;
    this->lastByteTimestamp = currentTime;

    // buffer the byte
    this->appendByte(byte);

    // process the buffered bytes
    this->processBuffer();
}

void FrameDecoder::checkTimeout(const std::chrono::milliseconds currentTime)
{
    // timeout must be enabled
    if (this->interByteTimeout == 0ms) {
        return;
    }

    // partial frame must exist
    if (this->getBufferedSize() == 0) {
        return;
    }

    // line must be silent for longer than the timeout
    if (currentTime - this->lastByteTimestamp <= this->interByteTimeout) {
        return;
    }

    // drop the stale bytes
    this->discardBytes(this->getBufferedSize(), true);

    // keep track of inter byte timeouts
    ++this->statistics.interByteTimeoutCounter;

    // notify listeners about the inter byte timeout
    this->handler.onDecodeError(Definitions::DecodeErrorType::INTER_BYTE_TIMEOUT);
}

void FrameDecoder::reset()
{
    // drop the buffered bytes
    this->bufferStart = 0;
    this->bufferEnd = 0;
    this->lastByteTimestamp = 0ms;

    // clear the statistics
    this->statistics = DecoderStatistics();
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
    // processing must not be running already
    if (this->processing) {
        return;
    }

    // walk the buffered bytes one frame candidate at a time
    this->processing = true;
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

        // version must be a protocol version with its header size
        const uint8_t *candidate = this->buffer.data() + this->bufferStart;
        const Frame::Frame frameHeader(candidate, Definitions::HEADER_SIZE);
        const size_t headerSize = Definitions::getHeaderSize(frameHeader.getVersion());
        if (headerSize == 0) {
            // keep track of invalid versions
            ++this->statistics.invalidVersionCounter;

            // reject the candidate and scan again
            this->rejectCandidate(Definitions::DecodeErrorType::INVALID_VERSION);
            continue;
        }

        // payload must fit into a frame of this build
        const size_t payloadLength = frameHeader.getPayloadLength();
        if (payloadLength > Settings::MaximumPayloadSize) {
            // keep track of oversized payloads
            ++this->statistics.payloadTooLongCounter;

            // reject the candidate and scan again
            this->rejectCandidate(Definitions::DecodeErrorType::PAYLOAD_TOO_LONG);
            continue;
        }

        // payload and CRC32 must be buffered behind the header of the version
        const size_t frameSize = headerSize + Definitions::CRC_SIZE + payloadLength;
        if (this->getBufferedSize() < frameSize) {
            break;
        }

        // CRC32 must match the frame
        const Frame::Frame frame(candidate, frameSize);
        if (!frame.getCRCValid()) {
            // keep track of CRC32 mismatches
            ++this->statistics.crcMismatchCounter;

            // reject the candidate and scan again
            this->rejectCandidate(Definitions::DecodeErrorType::CRC_MISMATCH);
            continue;
        }

        // consume the frame
        this->discardBytes(frameSize, false);

        // keep track of frames
        ++this->statistics.incomingFrameCounter;

        // notify listeners about the received frame
        this->handler.onReceivedFrame(frame);
    }
    this->processing = false;
}

void FrameDecoder::rejectCandidate(const Definitions::DecodeErrorType decodeErrorType)
{
    // NOTE: the real frame may start inside the rejected candidate
    this->discardBytes(1, true);

    // notify listeners about the decode error
    this->handler.onDecodeError(decodeErrorType);
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

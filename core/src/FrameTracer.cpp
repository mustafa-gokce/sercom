#include "../include/FrameTracer.h"

#include <algorithm>

namespace sercom::FrameTracer {

std::string_view fromTraceDirectionType(const TraceDirectionType traceDirectionType)
{
    switch (traceDirectionType) {
    case TraceDirectionType::HOST_TO_DEVICE: {
        return {"HOST_TO_DEVICE"};
    }
    case TraceDirectionType::DEVICE_TO_HOST: {
        return {"DEVICE_TO_HOST"};
    }
    case TraceDirectionType::UNKNOWN:
    default: {
        return {"UNKNOWN"};
    }
    }
}

TraceDirectionType toTraceDirectionType(const std::string_view traceDirectionType)
{
    if (traceDirectionType == std::string_view("HOST_TO_DEVICE")) {
        return TraceDirectionType::HOST_TO_DEVICE;
    }
    if (traceDirectionType == std::string_view("DEVICE_TO_HOST")) {
        return TraceDirectionType::DEVICE_TO_HOST;
    }
    return TraceDirectionType::UNKNOWN;
}

std::string_view getTraceDirectionTypeText(const TraceDirectionType traceDirectionType)
{
    switch (traceDirectionType) {
    case TraceDirectionType::HOST_TO_DEVICE: {
        return {">>"};
    }
    case TraceDirectionType::DEVICE_TO_HOST: {
        return {"<<"};
    }
    case TraceDirectionType::UNKNOWN:
    default: {
        return {"??"};
    }
    }
}

bool isTextData(const uint8_t *data, const size_t size)
{
    // data must exist
    if (data == nullptr) {
        return false;
    }

    // expose whether every byte belongs to a line of text
    return std::all_of(data, data + size, [](const uint8_t byte) {
        return (byte >= ' ' && byte <= '~') || byte == '\r' || byte == '\n' || byte == '\t';
    });
}

FrameTracerHandler::~FrameTracerHandler() {}

TraceChannel::TraceChannel(FrameTracerHandler &handler, const TraceDirectionType traceDirectionType)
    : handler(handler)
    , traceDirectionType(traceDirectionType)
    , frameDecoder(*this, Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC)
    , buffer()
    , bufferedSize(0)
    , takenDiscardedByteCounter(0)
    , reportedByteCount(0)
    , unframedData()
    , unframedDataSize(0)
    , unframedDataTimestamp(0)
    , lastByteTimestamp(0)
{}

TraceChannel::~TraceChannel() {}

void TraceChannel::onReceivedData(const uint8_t *data,
                                  const size_t size,
                                  const std::chrono::milliseconds currentTime)
{
    // data must exist
    if (data == nullptr) {
        return;
    }

    // iterate over the traced bytes
    std::for_each(data, data + size, [this, currentTime](const uint8_t byte) {
        // byte must fit into the buffer
        if (this->bufferedSize == this->buffer.size()) {
            return;
        }

        // buffer the byte together with the decoder
        this->buffer[this->bufferedSize] = byte;
        ++this->bufferedSize;
        this->lastByteTimestamp = currentTime;
        this->frameDecoder.onReceivedByte(byte, currentTime);

        // take the bytes that the decoder discarded
        this->takeDiscardedBytes(this->getDiscardedByteCount());
    });
}

void TraceChannel::checkTimeout(const std::chrono::milliseconds currentTime)
{
    // drop a partial frame of a silent line
    this->frameDecoder.checkTimeout(currentTime);
    this->takeDiscardedBytes(this->getDiscardedByteCount());

    // line must be silent for longer than the inter byte timeout
    if (currentTime - this->lastByteTimestamp <= Settings::DEFAULT_INTER_BYTE_TIMEOUT_MSEC) {
        return;
    }

    // report the unframed bytes of the silent line
    this->reportUnframedData();
}

void TraceChannel::onReceivedFrame(const Frame::Frame &frame)
{
    // take and report the unframed bytes before the frame
    this->takeDiscardedBytes(this->getDiscardedByteCount());
    this->reportUnframedData();

    // remove the bytes of the frame
    this->removeBytes(frame.getSize());

    // notify listeners about the frame
    this->handler.onReceivedTracedFrame(this->traceDirectionType, frame, this->lastByteTimestamp);
}

void TraceChannel::onDecodeError(const Definitions::DecodeErrorType decodeErrorType)
{
    // decode error must be a CRC32 mismatch
    if (decodeErrorType != Definitions::DecodeErrorType::CRC_MISMATCH) {
        return;
    }

    // first byte of the candidate must be discarded
    const size_t discardedByteCount = this->getDiscardedByteCount();
    if (discardedByteCount == 0) {
        return;
    }

    // take the unframed bytes before the candidate
    this->takeDiscardedBytes(discardedByteCount - 1);

    // header of the candidate must be buffered
    if (this->bufferedSize < Definitions::HEADER_SIZE) {
        return;
    }

    // whole candidate must be buffered
    const Frame::Frame frameHeader(this->buffer.data(), Definitions::HEADER_SIZE);
    const size_t frameSize = Definitions::FRAME_OVERHEAD + frameHeader.getPayloadLength();
    if (frameSize > this->bufferedSize) {
        return;
    }

    // report the unframed bytes before the candidate
    this->reportUnframedData();

    // do not report the bytes of the candidate again
    this->reportedByteCount = std::max(this->reportedByteCount, frameSize);

    // notify listeners about the candidate
    const Frame::Frame frame(this->buffer.data(), frameSize);
    this->handler.onReceivedTracedFrame(this->traceDirectionType, frame, this->lastByteTimestamp);
}

size_t TraceChannel::getDiscardedByteCount() const
{
    // expose the discarded bytes that are not taken yet
    return static_cast<size_t>(this->frameDecoder.getStatistics().discardedByteCounter
                               - this->takenDiscardedByteCounter);
}

void TraceChannel::takeDiscardedBytes(const size_t byteCount)
{
    // take at most the buffered bytes
    const size_t takenByteCount = std::min(byteCount, this->bufferedSize);

    // iterate over the taken bytes that are not reported yet
    const size_t firstUnreportedIndex = std::min(this->reportedByteCount, takenByteCount);
    std::for_each(this->buffer.cbegin() + firstUnreportedIndex,
                  this->buffer.cbegin() + takenByteCount,
                  [this](const uint8_t byte) { this->appendUnframedByte(byte); });

    // keep track of taken bytes
    this->takenDiscardedByteCounter += takenByteCount;
    this->removeBytes(takenByteCount);
}

void TraceChannel::removeBytes(const size_t byteCount)
{
    // move the remaining bytes to the front
    const size_t removedByteCount = std::min(byteCount, this->bufferedSize);
    std::copy(this->buffer.cbegin() + removedByteCount,
              this->buffer.cbegin() + this->bufferedSize,
              this->buffer.begin());
    this->bufferedSize -= removedByteCount;

    // forget the reported bytes that are removed
    this->reportedByteCount -= std::min(this->reportedByteCount, removedByteCount);
}

void TraceChannel::appendUnframedByte(const uint8_t byte)
{
    // remember the arrival time of the first unframed byte
    if (this->unframedDataSize == 0) {
        this->unframedDataTimestamp = this->lastByteTimestamp;
    }

    // store the unframed byte
    this->unframedData[this->unframedDataSize] = byte;
    ++this->unframedDataSize;

    // report a line of text at its line feed
    static constexpr uint8_t lineFeed = '\n';
    if (byte == lineFeed && isTextData(this->unframedData.data(), this->unframedDataSize)) {
        this->reportUnframedData();
        return;
    }

    // report the unframed bytes when they are full
    if (this->unframedDataSize == this->unframedData.size()) {
        this->reportUnframedData();
    }
}

void TraceChannel::reportUnframedData()
{
    // unframed bytes must exist
    if (this->unframedDataSize == 0) {
        return;
    }

    // notify listeners about the unframed bytes
    const size_t currentUnframedDataSize = this->unframedDataSize;
    this->unframedDataSize = 0;
    this->handler.onReceivedUnframedData(this->traceDirectionType,
                                         this->unframedData.data(),
                                         currentUnframedDataSize,
                                         this->unframedDataTimestamp);
}

FrameTracer::FrameTracer(FrameTracerHandler &handler,
                         const Clock::Clock &clock,
                         const TraceDirectionType sentTraceDirectionType)
    : clock(clock)
    , sentTraceChannel(handler, sentTraceDirectionType)
    , receivedTraceChannel(handler,
                           sentTraceDirectionType == TraceDirectionType::HOST_TO_DEVICE
                               ? TraceDirectionType::DEVICE_TO_HOST
                               : TraceDirectionType::HOST_TO_DEVICE)
{}

FrameTracer::~FrameTracer() {}

void FrameTracer::onSentData(const uint8_t *data, const size_t size)
{
    // trace the sent data
    this->sentTraceChannel.onReceivedData(data, size, this->clock.getTime());
}

void FrameTracer::onReceivedData(const uint8_t *data, const size_t size)
{
    // trace the received data
    this->receivedTraceChannel.onReceivedData(data, size, this->clock.getTime());
}

void FrameTracer::maintain()
{
    // check the inter byte timeouts of both directions
    const std::chrono::milliseconds currentTime = this->clock.getTime();
    this->sentTraceChannel.checkTimeout(currentTime);
    this->receivedTraceChannel.checkTimeout(currentTime);
}

} // namespace sercom::FrameTracer

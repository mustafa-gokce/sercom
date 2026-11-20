#ifndef SERCOM_FRAME_DECODER_H
#define SERCOM_FRAME_DECODER_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "../include/Definitions.h"
#include "../include/Frame.h"
#include "../include/Settings.h"

namespace sercom::FrameDecoder {

/*
 * Decoder statistics since construction.
 */
struct DecoderStatistics
{
    uint64_t incomingByteCounter = 0;  // bytes handed to the decoder
    uint64_t discardedByteCounter = 0; // bytes that did not end up in a valid frame
    uint64_t incomingFrameCounter = 0; // frames that passed every check
};

class FrameDecoderHandler
{
public:
    /*
     * Receiver of decoder results, implemented by the layer above the decoder.
     */
    FrameDecoderHandler() = default;
    virtual ~FrameDecoderHandler();

    // delete copy constructor and copy assignment operator
    FrameDecoderHandler(const FrameDecoderHandler &) = delete;
    FrameDecoderHandler &operator=(const FrameDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    FrameDecoderHandler(FrameDecoderHandler &&) = delete;
    FrameDecoderHandler &operator=(FrameDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame that passed every check.
     * The frame reference is valid during the call only.
     */
    virtual void onReceivedFrame(const Frame::Frame &frame) = 0;

    /*
     * Listen for a rejected frame candidate.
     */
    virtual void onDecodeError(Definitions::DecodeErrorType decodeErrorType) = 0;
};

class FrameDecoder final
{
public:
    /*
     * Decoder of a byte stream into frames that finds the sync word again after any error.
     */
    explicit FrameDecoder(FrameDecoderHandler &handler);
    ~FrameDecoder();

    // delete copy constructor and copy assignment operator
    FrameDecoder(const FrameDecoder &) = delete;
    FrameDecoder &operator=(const FrameDecoder &) = delete;

    // delete move constructor and move assignment operator
    FrameDecoder(FrameDecoder &&) = delete;
    FrameDecoder &operator=(FrameDecoder &&) = delete;

public:
    /*
     * Listen for received data and notify the handler about every result.
     */
    void onReceivedData(const uint8_t *data, size_t size, std::chrono::milliseconds currentTime);

    /*
     * Listen for a received byte and notify the handler about every result.
     */
    void onReceivedByte(uint8_t byte, std::chrono::milliseconds currentTime);

public:
    /*
     * Get the decoder statistics.
     */
    [[nodiscard]] DecoderStatistics getStatistics() const;

    /*
     * Get the number of buffered bytes that wait to become a frame.
     */
    [[nodiscard]] size_t getBufferedSize() const;

private:
    /*
     * Process the buffered bytes until more bytes are needed.
     */
    void processBuffer();

    /*
     * Remove bytes from the front of the buffer.
     */
    void discardBytes(size_t byteCount, bool countAsDiscarded);

    /*
     * Append a byte to the end of the buffer.
     */
    void appendByte(uint8_t byte);

    /*
     * Get a buffered byte counted from the start of the current frame candidate.
     */
    [[nodiscard]] uint8_t getBufferedByte(size_t index) const;

private:
    FrameDecoderHandler &handler;                             // reference to handler
    std::array<uint8_t, Settings::MAXIMUM_FRAME_SIZE> buffer; // bytes of the frame candidate
    size_t bufferStart;                                       // index of the first buffered byte
    size_t bufferEnd;                                         // index after the last buffered byte
    std::chrono::milliseconds lastByteTimestamp;              // arrival time of the newest byte
    DecoderStatistics statistics;                             // decoder statistics
};

} // namespace sercom::FrameDecoder

#endif // SERCOM_FRAME_DECODER_H

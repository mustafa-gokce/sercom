#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../core/include/Settings.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that checks every received frame and counts its bytes.
     */
    MockDecoderHandler()
        : frameSize(0)
    {}
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame, check it and count its bytes.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        // payload length must respect the maximum payload size
        if (frame.getPayloadLength() > sercom::Settings::MaximumPayloadSize) {
            std::abort();
        }

        // rebuilt frame must have the same bytes
        const sercom::Frame::Frame builtFrame(frame.getFlags(),
                                              frame.getMessageCounter(),
                                              frame.getMessageType(),
                                              frame.getPayload(),
                                              frame.getPayloadLength());
        if (builtFrame.getSize() != frame.getSize()
            || !std::equal(frame.getData(),
                           frame.getData() + frame.getSize(),
                           builtFrame.getData())) {
            std::abort();
        }

        // keep track of the frame bytes
        this->frameSize += frame.getSize();
    }

    /*
     * Listen for a decode error that the decoder statistics count.
     */
    void onDecodeError(
        [[maybe_unused]] const sercom::Definitions::DecodeErrorType decodeErrorType) final
    {}

public:
    /*
     * Get the size of all received frames in bytes.
     */
    [[nodiscard]] size_t getFrameSize() const
    {
        // expose the frame size
        return this->frameSize;
    }

private:
    size_t frameSize; // size of all received frames in bytes
};

/*
 * Fuzz entry point of the frame decoder.
 */
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, const size_t size)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // input must hold the chunk size selector
    if (size == 0) {
        return 0;
    }

    // take the chunk size from the first byte
    static constexpr size_t maximumChunkSize = 64;
    const size_t chunkSize = static_cast<size_t>(data[0]) % maximumChunkSize + 1;
    const uint8_t *streamData = data + 1;
    const size_t streamSize = size - 1;

    // create the frame decoder without the inter byte timeout
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);

    // hand the stream to the frame decoder in chunks
    size_t position = 0;
    while (position < streamSize) {
        // hand the next chunk to the frame decoder
        const size_t receivedSize = std::min(chunkSize, streamSize - position);
        frameDecoder.onReceivedData(streamData + position, receivedSize, 0ms);
        position += receivedSize;
    }

    // every byte must be counted once
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    if (decoderStatistics.incomingByteCounter != streamSize) {
        std::abort();
    }

    // byte counts must add up to the stream size
    if (decoderStatistics.discardedByteCounter + mockDecoderHandler.getFrameSize()
            + frameDecoder.getBufferedSize()
        != streamSize) {
        std::abort();
    }

    // input processed successfully
    return 0;
}

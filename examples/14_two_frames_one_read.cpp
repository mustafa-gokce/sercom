#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "../app/include/Messages.h"
#include "../core/include/Definitions.h"
#include "../core/include/Frame.h"
#include "../core/include/FrameDecoder.h"
#include "../host/include/HostUtils.h"

class MockDecoderHandler final : public sercom::FrameDecoder::FrameDecoderHandler
{
public:
    /*
     * Mock decoder handler that prints every received frame and every decode error.
     */
    MockDecoderHandler() = default;
    ~MockDecoderHandler() final {}

    // delete copy constructor and copy assignment operator
    MockDecoderHandler(const MockDecoderHandler &) = delete;
    MockDecoderHandler &operator=(const MockDecoderHandler &) = delete;

    // delete move constructor and move assignment operator
    MockDecoderHandler(MockDecoderHandler &&) = delete;
    MockDecoderHandler &operator=(MockDecoderHandler &&) = delete;

public:
    /*
     * Listen for a received frame and print its message counter.
     */
    void onReceivedFrame(const sercom::Frame::Frame &frame) final
    {
        using namespace sercom;

        // print the message counter
        const std::string line = "  handler: frame received, message counter "
                                 + std::to_string(frame.getMessageCounter());
        HostUtils::printLine(line);
    }

    /*
     * Listen for a decode error and print its type.
     */
    void onDecodeError(const sercom::Definitions::DecodeErrorType decodeErrorType) final
    {
        using namespace sercom;

        // print the decode error
        const std::string line = "  handler: decode error "
                                 + std::string(Definitions::fromDecodeErrorType(decodeErrorType));
        HostUtils::printLine(line);
    }
};

/*
 * Create a frame with a ping request that carries the message counter.
 */
[[nodiscard]] sercom::Frame::Frame createPingFrame(uint16_t messageCounter);

/*
 * Hand one read to the decoder and print its size and the bytes that the decoder holds after it.
 */
void receiveRead(sercom::FrameDecoder::FrameDecoder &frameDecoder,
                 std::string_view name,
                 const std::vector<uint8_t> &data);

/*
 * Main entry point of the executable.
 */
int main(int argc, char *argv[]);

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create three frames
    static constexpr uint16_t firstMessageCounter = 1;
    static constexpr uint16_t secondMessageCounter = 2;
    static constexpr uint16_t thirdMessageCounter = 3;
    const Frame::Frame firstFrame = createPingFrame(firstMessageCounter);
    const Frame::Frame secondFrame = createPingFrame(secondMessageCounter);
    const Frame::Frame thirdFrame = createPingFrame(thirdMessageCounter);

    // create the decoder without the inter byte timeout
    MockDecoderHandler mockDecoderHandler;
    FrameDecoder::FrameDecoder frameDecoder(mockDecoderHandler, 0ms);

    // receive the first two frames back to back in one read
    HostUtils::printLine("two frames in one read:");
    std::vector<uint8_t> firstRead(firstFrame.getData(),
                                   firstFrame.getData() + firstFrame.getSize());
    firstRead.insert(firstRead.end(),
                     secondFrame.getData(),
                     secondFrame.getData() + secondFrame.getSize());
    receiveRead(frameDecoder, "read 1", firstRead);

    // receive the third frame split over two reads
    HostUtils::printLine("one frame in two reads:");
    static constexpr size_t splitOffset = 5;
    const std::vector<uint8_t> secondRead(thirdFrame.getData(), thirdFrame.getData() + splitOffset);
    const std::vector<uint8_t> thirdRead(thirdFrame.getData() + splitOffset,
                                         thirdFrame.getData() + thirdFrame.getSize());
    receiveRead(frameDecoder, "read 2", secondRead);
    receiveRead(frameDecoder, "read 3", thirdRead);

    // decoder must find all three frames without discarding a byte
    static constexpr uint64_t frameCount = 3;
    const FrameDecoder::DecoderStatistics decoderStatistics = frameDecoder.getStatistics();
    if (decoderStatistics.incomingFrameCounter != frameCount
        || decoderStatistics.discardedByteCounter != 0) {
        HostUtils::logWarning("The decoder did not find all three frames");
        return 1;
    }

    // example finished successfully
    return 0;
}

sercom::Frame::Frame createPingFrame(const uint16_t messageCounter)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // create the frame with a ping request
    const Messages::PingRequest pingRequest(0ms);
    const Frame::Frame frame(0,
                             messageCounter,
                             Messages::fromMessageType(Messages::MessageType::PING_REQUEST),
                             pingRequest.getPayload(),
                             pingRequest.getPayloadLength());

    // expose the frame
    return frame;
}

void receiveRead(sercom::FrameDecoder::FrameDecoder &frameDecoder,
                 const std::string_view name,
                 const std::vector<uint8_t> &data)
{
    using namespace sercom;
    using namespace std::chrono_literals;

    // print the size of the read
    const std::string readLine = std::string(name) + ": " + std::to_string(data.size()) + " bytes";
    HostUtils::printLine(readLine);

    // hand the read to the decoder
    frameDecoder.onReceivedData(data.data(), data.size(), 0ms);

    // print the bytes that wait for the rest of their frame
    const std::string bufferedLine = "  decoder holds "
                                     + std::to_string(frameDecoder.getBufferedSize())
                                     + " bytes after " + std::string(name);
    HostUtils::printLine(bufferedLine);
}
